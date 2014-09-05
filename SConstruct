import os
import sys
import SCons

from os.path import basename, dirname, exists, isdir, isfile, join as joinpath

#
#  Setup command line options for different build types
#
AddOption('--verbose', dest='verbose', action='store_true',
          help='Show full compiler command line')
AddOption('--build-type', dest='build_type', type='choice',
          choices=["debug","fast","prof"],
          help='Type of build. Determines compiler flags')


#
#  Setup the default build environment.
#  This environment will be copied for different build types
#  e.g., release, debug, profiling, etc.
#
env = Environment(ENV = os.environ)

build_type = GetOption("build_type")

if build_type == None or build_type == "fast":
    env.Append(CCFLAGS='-O3')
    env.Append(CCFLAGS='-Werror')
    env.Append(CCFLAGS='-Wall')
    env.Append(CCFLAGS='-Wextra')
    env.Append(CCFLAGS='-Woverloaded-virtual')
    env.Append(CCFLAGS='-fPIC')
    env.Append(CCFLAGS='-std=c++0x')
    env.Append(CCFLAGS='-DNDEBUG')
    env['OBJSUFFIX'] = '.fo'
    build_type = "fast"
elif build_type == "debug":
    env.Append(CCFLAGS='-O0')
    env.Append(CCFLAGS='-ggdb3')
    env.Append(CCFLAGS='-Werror')
    env.Append(CCFLAGS='-Wall')
    env.Append(CCFLAGS='-Wextra')
    env.Append(CCFLAGS='-Woverloaded-virtual')
    env.Append(CCFLAGS='-fPIC')
    env.Append(CCFLAGS='-std=c++0x')
    env['OBJSUFFIX'] = '.do'
elif build_type == "prof":
    env.Append(CCFLAGS='-O0')
    env.Append(CCFLAGS='-ggdb3')
    env.Append(CCFLAGS='-pg')
    env.Append(CCFLAGS='-Werror')
    env.Append(CCFLAGS='-Wall')
    env.Append(CCFLAGS='-Wextra')
    env.Append(CCFLAGS='-Woverloaded-virtual')
    env.Append(CCFLAGS='-fPIC')
    env.Append(CCFLAGS='-std=c++0x')
    env.Append(CCFLAGS='-DNDEBUG')
    env.Append(LINKFLAGS='-pg')
    env['OBJSUFFIX'] = '.po'


env['BUILDROOT'] = "build"
env['NVMAIN_BUILD'] = "trace"

env.Append(CPPPATH=Dir('.'))
env.Append(CCFLAGS='-DTRACE')
env.srcdir = Dir(".")
env.SetOption("duplicate", "soft-copy")
base_dir = env.srcdir.abspath

Export('env')


#
#  All the sources for the project will be appended to this
#  list. At the end of the script, the program will be built
#  as prog_name from this list. This can be customized to add
#  different sources to different source lists.
#
src_list = []
prog_name = 'nvmain'


#
#  Defines a function used in hierarchical SConscripts that
#  adds to the master source list. 
#
def NVMainSource(src):
    src_list.append(File(src))
Export('NVMainSource')

#
#  The following functions are for customizing the build
#  output messages. These are completely optional. I am
#  using the format shown below:
#
#  [TOOL] Verbing Component_Type "Component_Name" -> Output_Name
#
#  Where tool is the build tool, e.g., CC, CXX, AR, LINK, etc.,
#  Verbing is a verb for the tool, e.g., Compiling, Linking, etc.,
#  Component_Type is the base class for the class being built,
#  Component_Name is the name of the class' file, and Output_Name
#  is the name of the object file being generated.
#

source_types = {
    'MemControl'   : "Memory Controller",
    'FaultModels'  : "Hard-Error Model",
    'NVM'          : "Source",
    'SimInterface' : "Simulation Interface",
    'Utils'        : "Utility Feature",
    'include'      : "Include",
    'src'          : "Source",
    'traceReader'  : "Trace Reader",
    'traceSim'     : "Trace main()",
    prog_name      : "Program"
}

#
#  source_type will return a string corresponding to the component
#  type. Customize based on your project.
#
def source_type(build_dir, src_name):
    # Default component type
    type = "Source"
     
    if build_dir in source_types:
        type = source_types[build_dir]
    
    # Highlight factory classes
    if src_name[-7:] == "Factory":
        type = type + " Factory"
    
    return type

#
#  SConscripts can add a component type as well
#
def NVMainSourceType(build_dir, src_type):
    source_types[build_dir] = src_type
Export('NVMainSourceType')


#
#  Setup some colors for terminals that support colors.
#
colors = {}
colors['cyan']   = '\033[96m'
colors['purple'] = '\033[95m'
colors['blue']   = '\033[94m'
colors['green']  = '\033[92m'
colors['yellow'] = '\033[93m'
colors['red']    = '\033[91m'
colors['normal'] = '\033[0m'

if not sys.stdout.isatty():
    for key, value in colors.iteritems():
        colors[key] = ''


#
#  Generate a string of the form:
#
#  [TOOL] Verbing Component_Type "Component_Name" -> Output_Name
#
#  when __call__ed.
#
class PrettyPrint(object):

    tool_color  = colors['normal']
    verb_color  = colors['normal']
    type_color  = colors['normal']
    src_color   = colors['blue']
    arrow_color = colors['red']
    out_color   = colors['blue']

    def __init__(cls, tool, verb, show_src=True):
        cls.output_str = cls.tool_color + ("[%s] " % tool) \
                       + cls.verb_color + ("%s " % verb) \
                       + cls.type_color + "%s " \
                       + cls.src_color + "\"%s\"" \
                       + cls.arrow_color + " ==> " \
                       + cls.out_color + "%s" \
                       + colors['normal']

        cls.show_src = show_src

    def __call__(cls, target, source, env, for_signature=None):

        if not cls.show_src:
            source = []

        def strip(f):
            return strip_build_path(str(f), env)

        if len(source) > 0:
            srcs = map(strip, source)
            tgts = map(strip, target)
            split_src = srcs[0].split("/")

            src_basename = os.path.splitext(split_src[-1])[0]
            src_name = src_basename
            suffix = env['OBJSUFFIX']
        else:
            tgts = map(strip, target)
            split_src = tgts[0].split("/")
            src_basename = os.path.splitext(split_src[-1])[0]
            src_name = tgts[0]
            suffix = ''

        src_type = source_type(split_src[0], src_basename)

        return cls.output_str % (src_type, src_basename, ("%s%s" % (src_name, suffix)) )

Export('PrettyPrint')

def strip_build_path(path, env):
    path = str(path)
    variant_base = env['BUILDROOT'] + os.path.sep
    if path.startswith(variant_base):
        path = path[len(variant_base):]
    elif path.startswith('build/'):
        path = path[6:]
    return path

#
#  Define output strings for different stages of the build. Add 
#  additional build steps here, e.g., QT_MOCCOMSTR, M4COMSTR, etc.
#
if GetOption("verbose") != True:
    MakeAction = Action
    env['CCCOMSTR']        = PrettyPrint("CC", "Compiling")
    env['CXXCOMSTR']       = PrettyPrint("CXX", "Compiling")
    env['ASCOMSTR']        = PrettyPrint("AS", "Assembling")
    env['ARCOMSTR']        = PrettyPrint("AR", "Archiving", False)
    env['LINKCOMSTR']      = PrettyPrint("LINK", "Linking", False)
    env['RANLIBCOMSTR']    = PrettyPrint("RANLIB", "Indexing Archive", False)
    Export('MakeAction')


#
#  Walk the current directory looking for SConscripts in each
#  subdirectory defining source files for that subdirectory.
#
obj_dir = joinpath(base_dir, env['BUILDROOT'])
for root, dirs, files in os.walk(base_dir, topdown=True):
    if root.startswith(obj_dir):
        # Don't check the build folder if it already exists from a prior build
        continue

    if 'SConscript' in files:
        build_dir = joinpath(env['BUILDROOT'], root[len(base_dir) + 1:])
        SConscript(joinpath(root, 'SConscript'), variant_dir=build_dir)


#
#  Build the name of the final output binary
#
final_bin = "%s.%s" % (prog_name, build_type)

NVMainSourceType(final_bin, "Program")

#
#  Build each of the programs with the corresponding source list.
#
env.Program(final_bin, src_list) 


