#!/usr/bin/python

import sys,collections

def extract_backoff(context_list, order):
  assert len(context_list) == (2*order)
  backoffs = []
  for i in range(1,order+1):
    if i == order:
      backoffs.append(([context_list[i-1]+"|"], ["|"+context_list[i]]))
    else:
      right_limit = 2*order-i
      core = context_list[i:right_limit]
      left = [context_list[i-1]+"|"*(order-i+1)]
      right = ["|"*(order-i+1)+context_list[right_limit]]
      backoffs.append((core, left, right))
# print context_list, backoffs
  return backoffs

def tuple_to_str(t):
  s=""
  for i,x in enumerate(t):
    if i > 0: s += "|"
    s += str(x)
  return s

if len(sys.argv) < 3:
  print "Usage: extract-contexts.py output_filename order cutoff lowercase"
  exit(1)

output_filename = sys.argv[1]
order = int(sys.argv[2])
cutoff = 0
if len(sys.argv) > 3:
  cutoff = int(sys.argv[3])
lowercase = False
if len(sys.argv) > 4:
  lowercase = bool(sys.argv[4])

contexts_dict={}
contexts_list=[]
contexts_freq=collections.defaultdict(int)
contexts_backoff={}

token_dict={}
token_list=[]
documents_dict=collections.defaultdict(dict)

contexts_at_order = [i for i in range(order+1)]

prefix = ["<s%d>|<s>"%i for i in range(order)]
suffix = ["</s%d>|</s>"%i for i in range(order)]

for line in sys.stdin:
  tokens = list(prefix)
  tokens.extend(line.split())
  tokens.extend(suffix)
  if lowercase:
    tokens = map(lambda x: x.lower(), tokens)

  for i in range(order, len(tokens)-order):
    context_list = []
    term=""
    for j in range(i-order, i+order+1):
      token,tag = tokens[j].rsplit('|',2)
      if j != i:
        context_list.append(token)
      else:
        if token not in token_dict: 
          token_dict[token] = len(token_dict)
          token_list.append(token)
        term = token_dict[token] 

    context = tuple_to_str(tuple(context_list))

    if context not in contexts_dict: 
      context_index = len(contexts_dict)
      contexts_dict[context] = context_index
      contexts_list.append(context)
      contexts_at_order[0] += 1

      # handle backoff
      backoff_contexts = extract_backoff(context_list, order)
      bo_indexes=[(context_index,)]
#     bo_indexes=[(context,)]
      for i,bo in enumerate(backoff_contexts):
        factor_indexes=[]
        for factor in bo:
          bo_tuple = tuple_to_str(tuple(factor))
          if bo_tuple not in contexts_dict:
            contexts_dict[bo_tuple] = len(contexts_dict)
            contexts_list.append(bo_tuple)
            contexts_at_order[i+1] += 1
#         factor_indexes.append(bo_tuple)
          factor_indexes.append(contexts_dict[bo_tuple])
        bo_indexes.append(tuple(factor_indexes))
      
      for i in range(len(bo_indexes)-1):
        contexts_backoff[bo_indexes[i][0]] = bo_indexes[i+1]

    context_index = contexts_dict[context]
    contexts_freq[context_index] += 1

    if context_index not in documents_dict[term]:
      documents_dict[term][context_index] = 1
    else:
      documents_dict[term][context_index] += 1

term_file = open(output_filename+".terms",'w')
for t in token_list: print >>term_file, t
term_file.close()

contexts_file = open(output_filename+".contexts",'w')
for c in contexts_list: 
  print >>contexts_file, c
contexts_file.close()

data_file = open(output_filename+".data",'w')
for t in range(len(token_list)): 
  line=""
  num_active=0
  for c in documents_dict[t]:
    count = documents_dict[t][c]
    if contexts_freq[c] >= cutoff:
      line += (' ' + str(c) + ':' + str(count))
      num_active += 1
  if num_active > 0:
    print >>data_file, "%d%s" % (num_active,line)
data_file.close()

contexts_backoff_file = open(output_filename+".contexts_backoff",'w')
print >>contexts_backoff_file, len(contexts_list), order,
#for x in contexts_at_order: 
#  print >>contexts_backoff_file, x,
#print >>contexts_backoff_file
for x in range(order-1):
  print >>contexts_backoff_file, 3,
print >>contexts_backoff_file, 2

for x in contexts_backoff: 
  print >>contexts_backoff_file, x, 
  for y in contexts_backoff[x]: print >>contexts_backoff_file, y,
  print >>contexts_backoff_file 
contexts_backoff_file.close()
