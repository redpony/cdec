bins = []
bin_sz = 0
1.upto(3).each { |i|
  bin_sz = STDIN.gets.strip.split(",")[1].to_i 
  bins.push [[i], bin_sz]
}

cur_bin = []
cur_bin_sz = 0
while line = STDIN.gets
  count, countcount = line.strip.split ","
  count = count.to_i
  countcount = countcount.to_i
  if (cur_bin_sz + countcount) > bin_sz 
    bins.push [cur_bin, cur_bin_sz]
    cur_bin = []
    cur_bin_sz = countcount
  else
    cur_bin.push count
    cur_bin_sz += countcount
  end
end
bins.push [cur_bin, cur_bin_sz]

c = 0
e = 0
bins.each { |i|
  puts "#{e} | #{i[0].size}: #{i[0][0]}.. #{i[1]}" if i[0].size > 0
  c += 1 if i[0].size > 0
  e += 1
}
puts "#{c} bins (#{bins.size})"
puts "bin sz #{bin_sz}"


