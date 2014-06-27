#!/usr/bin/python


from optparse import OptionParser
from array    import *
import sys



parser = OptionParser()
parser.add_option("-f","--files", help="NVMain out files to read")
parser.add_option("-i","--interval", default="null", help="Interval(s) to output")
parser.add_option("-c","--common", action="store_true", help="Use max common interval across files")
parser.add_option("-o","--output", help="File to write CSV data to")
parser.add_option("-s","--stats", help="File with list of statistics to collect from NVMain output")
parser.add_option("-m","--m5stats", default="null", help="File with list of statistics to collect from gem5 output")

(options, args) = parser.parse_args()

files = options.files.split(',')


dump_interval = [0] * len(options.files.split(','))


# Find max common interval
#if options.common:
max_int = [0] * len(options.files.split(','))
index = 0
common_interval = 9999999

for f in files:
    s = 'Finding max interval in file ' + f
    print s

    handle = open(f, 'r')
    for line in handle:
        tags = line.split('.')
        interval_tag = tags[0]
        if interval_tag[0] == 'i' and interval_tag[1].isdigit():
            interval = int(interval_tag[1:])
            if interval > max_int[index]:
                max_int[index] = interval

    s = f + ' has ' + str(max_int[index]) + ' intervals'
    print s

    if max_int[index] < common_interval:
        common_interval = max_int[index]

    index = index + 1
    handle.close()

s = 'Common interval is ' + str(common_interval)
print s

if options.common:
    index = 0
    for asdf in files:
        dump_interval[index] = common_interval
        index = index + 1
else:
    index = 0
    for asdf in files:
        dump_interval[index] = max_int[index]
        index = index + 1

if options.interval != "null":
    dump_interval = options.interval.split(',')

slist = open(options.stats, 'r')

stringlist = [] 

index = 0
for line in slist:
    s = 'i' + str(dump_interval[index]) + '.' + line.strip()
    stringlist.append(s)


slist.close()

m5stringlist = []

if options.m5stats != "null":
    slist = open(options.m5stats, 'r')
    
    for line in slist:
        m5stringlist.append(line.strip())


csvfile = open(options.output, 'w')


csvfile.write(',')
for item in stringlist:
    csvfile.write(item)
    csvfile.write(',')
for item in m5stringlist:
    csvfile.write(item)
    csvfile.write(',')

csvfile.write('\n')


file_index = 0

for f in files:
    handle = open(f, 'r')
    
    valuelist = []
    m5valuelist = []
    m5findcount = [0] * len(m5stringlist)

    # Make a list of default values
    for item in stringlist:
        valuelist.append('_')
    for item in m5stringlist:
        m5valuelist.append('_')

    for line in handle:
        item_index = 0

        for item in stringlist:
            if line[0:len(item)] == item:
                #s = 'Found an item: ' + line.strip()
                #print s
                valuelist[item_index] = line[len(item)+1:].strip()

            item_index = item_index + 1

    if options.m5stats != "null":
        m5out = f[0:len(f)-4] + '/m5out/stats.txt'
        m5handle = open(m5out, 'r')

        for line in m5handle:
            item_index = 0

            for item in m5stringlist:
                if line[0:len(item)] == item and m5findcount[item_index] < dump_interval[file_index]:
                    split_line = line.split('#')
                    split_string = split_line[0]
                    m5valuelist[item_index] = split_string[len(item)+1:].lstrip().rstrip()
                    m5findcount[item_index] = m5findcount[item_index] + 1
                    item_index = item_index + 1

                item_index = item_index + 1

    csvfile.write(f)
    csvfile.write(',')
    for value in valuelist:
        csvfile.write(value)
        csvfile.write(',')

    for value in m5valuelist:
        csvfile.write(value)
        csvfile.write(',')

    csvfile.write('\n')

    handle.close()

    file_index = file_index + 1


csvfile.close()




