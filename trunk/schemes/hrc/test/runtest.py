"""\
Automatic HRC changes tests runner.
Validates parsing result for a set of source files against
previously collected data.

Advantage over Perl version:
- doesn't require diff utility
- works out of the box on Windows with Python installed

placed in public domain by techtonik // rainforce.org
"""


import os, sys
import copy
import difflib
import optparse
import subprocess
import unittest
from datetime import datetime
from os.path import abspath, dirname, join, normpath


# -- path setup --
tests_path = normpath(dirname(__file__))
root_path = join(tests_path, "..", "..")
with open(join(root_path, "path.properties")) as pf:
  prop_path = dict(
    [line.strip()[5:].split("=")
      for line in pf
        if line.startswith("path.")]
  )
# print prop_path

colorer_path = join(root_path, normpath(prop_path["colorer"]))
catalog_path = join(root_path, normpath(prop_path["catalog"]))
hrd_path = join(root_path, prop_path["hrd"])

colorer = join(colorer_path, "bin", "colorer") + " -c " + catalog_path
if not os.path.isfile(colorer):
  sys.exit("Error: No colorer in %s" % colorer_path)

valid_dir = normpath(join(tests_path, "_valid"))
# __2009-06-05_12-35
current_dir = datetime.today().strftime("__%Y-%m-%d_%H-%M")
if os.path.exists(current_dir):
  sys.exit("Exiting: Test dir already exists - %s" % current_dir)
os.mkdir(current_dir)


# -- args parsing --

usage = "%prog [--quick]"

opt = optparse.OptionParser(usage=usage, add_help_option=False)
opt.add_option("--quick", action="store_true", help="exclude /full/ dirs from testing")
opt.add_option("--help", action="store_true")
#opt.add_option("--list", action="store_true")
(options, args) = opt.parse_args()

if options.help:
  print __doc__
  opt.print_help()
  sys.exit(0)


# -- utility functions

def filediff(oldpath, newpath):
  """return diff output or empty string"""
  with open(oldpath, "rb") as of:
    ol = of.readlines()
  with open(newpath, "rb") as nf:
    nl = nf.readlines()
  diff = difflib.unified_diff(ol, nl, oldpath, newpath, n=1)
  return list(diff)


print "Running tests"

fail_log = open(join(current_dir, "fails.html"), "w")
fail_log.write(
"""
<html>
<head>
  <title>%s Colorer Test Results</title>
  <style type="text/css">
.testname {text-weight:bold}
  </style>
</head>
<body>
""" % current_dir)
  

test_list = []
# look for files for highlight tests
# skip dirs: current ".", ".svn", "_valid" and "__*" results
for root, dirs, files in os.walk(tests_path):
  for d in copy.copy(dirs):
    if d[0] in (".", "_"):
      dirs.remove(d)
  if options.quick:
    if "full" in dirs:
      dirs.remove("full")
  if root == tests_path:
    continue
  for name in files:
    test_list.append(normpath(join(root, name)))

failed = 0
for no, test in enumerate(test_list):
  print "Processing (%s/%s) %s" % (no+1, len(test_list), test)

  origname = join(valid_dir, test)
  outname = join(current_dir, "%s.html" % test)
  outdir = dirname(outname)

  fail_log.write('<div><pre class="testname">%s</pre><pre>' % test)

  if not os.path.exists(outdir):
    # print creating
    os.makedirs(outdir)

  cmd = '%s -ht "%s" -dc -dh -ln -o "%s"' % (colorer, test, outname)
  # print cmd
  ret = subprocess.call(cmd)
  if ret != 0:
    failed += 1
    print "Failed: colorer returned %s" % ret
    fail_log.write("Failed: colorer returned %s" % ret)
    # BUG: colorer doesn't return any error codes in some error cases
    #      like absent hrd catalogs or 
    fail_log.write('</pre><div>')
    continue

  # XXX: colorer.exe compiled with MinGW produces output with unix line ends
  #      but SVN checkout is made with unix ones. Converting lineends here
  #      until it's clear which lineends are generated by VS compiled version.
  if os.name == 'nt':
    with open(outname) as fr:
      lines = fr.readlines()
    with open(outname, "w") as fw:
      fw.writelines(lines)

  diff = filediff("%s.html" % origname, outname)
  for line in diff:
    fail_log.write(line)

  fail_log.write('</pre><div>')

fail_log.write('</body></html>')
fail_log.close()
print "Executed: %s, Failed: %s/%s%%" % (len(test_list), failed, (float(failed)/len(test_list)*100))


"""
# TODO / comments
# tune unittest differ to be 
# my $diff  = 'diff -U 1 -bB';

evaluate usefulness
 --full   - test all (default)
 --list   - test listed dirs
 file.lst - test list from file(s)

add timings

list types and detect which are not covered by tests

provide code coverage stats

"""