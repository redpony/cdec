#!/usr/bin/python

import sys,collections

def tuple_to_str(t):
  s=""
  for i,x in enumerate(t):
    if i > 0: s += "|"
    s += str(x)
  return s

if len(sys.argv) < 5:
  print "Usage: extract-contexts_test.py output_filename vocab contexts order lowercase"
  exit(1)

output_filename = sys.argv[1]
output = open(output_filename+".test_data",'w')

unk_term="-UNK-"
vocab_dict={}
for i,x in enumerate(file(sys.argv[2], 'r').readlines()): 
  vocab_dict[x.strip()]=i

contexts_dict={}
contexts_list=[]
for i,x in enumerate(file(sys.argv[3], 'r').readlines()): 
  contexts_dict[x.strip()]=i
  contexts_list.append(x.strip())

order = int(sys.argv[4])

lowercase = False
if len(sys.argv) > 5:
  lowercase = bool(sys.argv[5])
if lowercase: unk_term = unk_term.lower()

prefix = ["<s%d>|<s>"%i for i in range(order)]
suffix = ["</s%d>|</s>"%i for i in range(order)]

assert unk_term in vocab_dict
for line in sys.stdin:
  tokens = list(prefix)
  tokens.extend(line.split())
  tokens.extend(suffix)
  if lowercase:
    tokens = map(lambda x: x.lower(), tokens)

  for i in range(order, len(tokens)-order):
    context_list=[]
    term=""
    for j in range(i-order, i+order+1):
      token,tag = tokens[j].rsplit('|',2)
      if j != i:
        context_list.append(token)
      else:
        if token not in vocab_dict: 
          term = vocab_dict[unk_term] 
        else:
          term = vocab_dict[token] 
    context = tuple_to_str(context_list)
    if context not in contexts_dict: 
      contexts_dict[context] = len(contexts_dict)
      contexts_list.append(context)
    context_index = contexts_dict[context]
    print >>output, "%d:%d" % (term,context_index),
  print >>output
output.close()

contexts_file = open(output_filename+".test_contexts",'w')
for c in contexts_list: 
  print >>contexts_file, c
contexts_file.close()
