#!/usr/bin/python


from optparse import OptionParser
import subprocess
import json
import sys
import os
import shutil
import re


parser = OptionParser()
parser.add_option("-c", "--configs", type="string", help="Path to file with list of configurations to test.", default="TestConfigs")
parser.add_option("-n", "--no-gem5", action="store_true", help="Skip gem5 tests.")
parser.add_option("-g", "--gem5-path", type="string", help="Path to gem5 directory.")
parser.add_option("-a", "--arch", type="string", help="gem5 architecture to test.", default="X86")
parser.add_option("-b", "--build", type="string", help="NVMain standalone/gem5 build to test (e.g., *.fast, *.prof, *.debug)", default="fast")
parser.add_option("-t", "--tempfile", type="string", help="Temporary file to write test output.", default=".temp")
parser.add_option("-f", "--max-fuzz", type="float", help="Maximum percentage stat values can be wrong.", default="1.0") # No more than 1% difference

(options, args) = parser.parse_args()


#
# Make sure our nvmain executable is found.
#
nvmainexec = ".." + os.sep + "nvmain." + options.build

if not os.path.isfile(nvmainexec) or not os.access(nvmainexec, os.X_OK):
    print "Could not find Nvmain executable: '%s'" % nvmainexec
    print "Exiting..."
    sys.exit(1)



#
# Find out if we are testing with gem5 or not.
#
testgem5 = True

gem5path = os.environ['M5_PATH']
if options.gem5_path:
    gem5path = options.gem5_path

gem5exec = gem5path + os.sep + "build" + os.sep + options.arch + os.sep + "gem5." + options.build

if options.no_gem5:
    testgem5 = False


if not os.path.isfile(gem5exec) or not os.access(gem5exec, os.X_OK):
    print "Could not run gem5 executable: '%s'" % gem5exec
    print "Skipping gem5 tests."
    testgem5 = False


#
# Read in the list of config files to test
#
json_data = open('Tests.json')

testdata = json.load(json_data)


#
# Run all tests with each trace
#

for trace in testdata["traces"]:

    for idx, test in enumerate(testdata["tests"]):
        faillog = testdata["tests"][idx]["name"] + ".out"

        # Reset log each time for correct stat comparison
        testlog = open(options.tempfile, 'w')

        command = [nvmainexec, testdata["tests"][idx]["config"], trace, testdata["tests"][idx]["cycles"]]
        command.extend(testdata["tests"][idx]["overrides"].split(" "))
        sys.stdout.write("Testing " + testdata["tests"][idx]["name"] + " with " + trace + " ... ")
        sys.stdout.flush()

        #for arg in command:
        #    print arg,
        #print ""

        try:
            subprocess.check_call(command, stdout=testlog, stderr=subprocess.STDOUT)
        except subprocess.CalledProcessError as e:
            expectedrc = testdata["tests"][idx]["returncode"]
            if e.returncode != expectedrc:
                print "[Failed RC=%u]" % e.returncode
                shutil.copyfile(options.tempfile, faillog)
                continue

        testlog.close()

        checkcount = 0
        checkcounter = 0
        passedchecks = []
        
        for check in testdata["tests"][idx]["checks"]:
            checkcount = checkcount + 1

        with open(options.tempfile, 'r') as flog:
            for line in flog:
                for check in testdata["tests"][idx]["checks"]:
                    if check in line:
                        checkcounter = checkcounter + 1
                        passedchecks.append(check)
                    elif check[0] == 'i':  # Skip for general stat checks
                        # See if the stat is there, but the value is slightly off
                        checkstat = check.split(' ')[0]
                        fval = re.compile("[0-9.]")
                        checkvalue = float(''.join(c for c in check.split(' ')[1] if fval.match(c)))
                        if checkstat in line:
                            refvalue = float(''.join(c for c in line.split(' ')[1] if fval.match(c)))
                            try:
                                fuzz = max( (1.0 - (checkvalue / refvalue)) * 100.0, (1.0 - (refvalue / checkvalue)) * 100.0)
                                if fuzz < options.max_fuzz:
                                    checkcounter = checkcounter + 1
                                    passedchecks.append(check)
                                else:
                                    print "Stat '%s' has value '%s' while reference has '%s'. Fuzz = %f" % (checkstat, checkvalue, refvalue, fuzz)
                            except ZeroDivisionError:
                                print "Warning: Stat '%s' has reference value (%s) or check value (%s) of zero." % (checkstat, refvalue, checkvalue)

        if checkcounter == checkcount:
            print "[Passed %d/%d]" % (checkcounter, checkcount)
            shutil.copyfile(options.tempfile, faillog)
        else:
            print "[Failed %d/%d]" % (checkcounter, checkcount)
            shutil.copyfile(options.tempfile, faillog)

            for check in testdata["tests"][idx]["checks"]:
                if not check in passedchecks:
                    print "Check %s failed." % check



