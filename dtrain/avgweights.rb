#!/usr/bin/env ruby1.9.1


STDIN.set_encoding 'utf-8'

#shard_count_key = "__SHARD_COUNT__"

w = {}
c = {}
w.default = 0
c.default = 0
while line = STDIN.gets
  key, val = line.split /\t/
  w[key] += val.to_f
  c[key] += 1.0
end

#shard_count = w["__SHARD_COUNT__"]

w.each_key { |k|
  #if k == shard_count_key then next end
  #if k == "__bias" then next end
  puts "#{k}\t#{w[k]/c[k]}" #{w[k]/shard_count}"
}

#puts "#{shard_count_key}\t#{w[shard_count_key]}"

