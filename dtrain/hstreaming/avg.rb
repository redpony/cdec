#!/usr/bin/env ruby
# first arg may be an int of custom shard count

shard_count_key = "__SHARD_COUNT__"

STDIN.set_encoding 'utf-8'
STDOUT.set_encoding 'utf-8'

w = {}
c = {}
w.default = 0
c.default = 0
while line = STDIN.gets
  key, val = line.split /\s/
  w[key] += val.to_f
  c[key] += 1
end

if ARGV.size == 0
  shard_count = w["__SHARD_COUNT__"]
else
  shard_count = ARGV[0].to_f
end
w.each_key { |k|
  if k == shard_count_key
    next
  else
    puts "#{k}\t#{w[k]/shard_count}"
    #puts "# #{c[k]}"
  end
}

