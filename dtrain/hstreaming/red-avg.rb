#!/usr/bin/env ruby1.9.1

shard_count_key = "__SHARD_COUNT__"

STDIN.set_encoding 'utf-8'
STDOUT.set_encoding 'utf-8'

w = {}
c = {}
w.default = 0
c.default = 0
while line = STDIN.gets
  key, val = line.split /\t/
  w[key] += val.to_f
  c[key] += 1
end

puts "# dtrain reducer: average"
shard_count = w["__SHARD_COUNT__"]
w.each_key { |k|
  if k == shard_count_key
    puts "# shard count: #{shard_count.to_i}"
  else
    puts "#{k}\t#{w[k]/shard_count}\t# #{c[k]}"
  end
}

