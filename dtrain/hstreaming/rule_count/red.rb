#!/usr/bin/env ruby

STDIN.set_encoding 'utf-8'
STDOUT.set_encoding 'utf-8'

def output(key, val)
  puts "#{key}\t#{val}"
end

prev_key = nil
sum = 0
while line = STDIN.gets
   key, val = line.strip.split /\t/
   if key != prev_key && sum > 0
      output prev_key, sum
      prev_key = key
      sum = 0
   elsif !prev_key
      prev_key = key
   end
   sum += val.to_i
end
output prev_key, sum

