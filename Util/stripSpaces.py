import string, sys

fIn = sys.argv[1]

f = open(fIn)
for l in f:
  print ','.join([w.lstrip().rstrip() for w in l.split(',')])
