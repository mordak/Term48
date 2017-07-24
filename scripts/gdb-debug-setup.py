import os
import gdb

cwd = os.getcwd()
base_dir = os.environ['BASE_DIR_REPLACED'] # BBNDK directory
qnx_target = os.environ['QNX_TARGET']

bbip = os.environ.get('BBIP', None)
if not bbip:
    raise gdb.GdbError("Set target device IP in env. var. BBIP")
binary_path = os.environ.get('BINARY_PATH', None)
if not binary_path:
    raise gdb.GdbError("Set path to the target binary in env. var. BINARY_PATH")
binary_name = os.path.basename(binary_path)

solib_search_paths = map(lambda p: qnx_target + p, [
    "../target-override/armle-v7/lib",
    "../target-override/armle-v7/usr/lib",
    "../target-override/armle-v7/usr/lib/qt4/lib",
    "../target-override/armle-v7/usr/lib/qt5/lib",
    "/armle-v7/lib",
    "/armle-v7/usr/lib",
    "/armle-v7/usr/lib/qt4/lib",
    "/armle-v7/usr/lib/qt5/lib",
])

map(gdb.execute, [
    "cd %s" % cwd,
    "set breakpoint pending on",
    "enable pretty-printer",
    "source %sconfiguration/org.eclipse.osgi/bundles/100/1/.cp/printers/gdbinit" % base_dir,
    "set print object on",
    "set print sevenbit-strings on",
    "set auto-solib-add on",
    "set solib-search-path %s" % ";".join(solib_search_paths),
    "file %s" % binary_path,
    "target qnx %s:8000" % bbip
])

pids = filter(lambda l: binary_name in l, gdb.execute("info pidlist", to_string=True).split('\n'))
if not pids:
    raise gdb.GdbError("Make sure the target application has been started!")

[pid, thread_pid] = pids[0].split(' - ')
pid = thread_pid.split('/')[0]

map(gdb.execute, [
    "attach %s" % pid,
    "handle SIGTERM nostop noprint"
])
