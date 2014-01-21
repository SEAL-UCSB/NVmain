#!/usr/bin/python


from optparse import OptionParser
import subprocess
import json
import sys
import os
import shutil


parser = OptionParser()
parser.add_option("-c", "--configs", type="string", help="Path to file with list of configurations to test.", default="TestConfigs")
parser.add_option("-n", "--no-gem5", action="store_true", help="Skip gem5 tests.")
parser.add_option("-g", "--gem5-path", type="string", help="Path to gem5 directory.")
parser.add_option("-a", "--arch", type="string", help="gem5 architecture to test.", default="X86")
parser.add_option("-b", "--build", type="string", help="NVMain standalone/gem5 build to test (e.g., *.fast, *.prof, *.debug)", default="fast")
parser.add_option("-t", "--tempfile", type="string", help="Temporary file to write test output.", default=".temp")

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

        if checkcounter == checkcount:
            print "[Passed %d/%d]" % (checkcounter, checkcount)
        else:
            print "[Failed %d/%d]" % (checkcounter, checkcount)
            shutil.copyfile(options.tempfile, faillog)

            for check in testdata["tests"][idx]["checks"]:
                if not check in passedchecks:
                    print "Check %s failed." % check



