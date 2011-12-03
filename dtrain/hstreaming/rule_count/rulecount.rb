STDIN.set_encoding 'utf-8'
STDOUT.set_encoding 'utf-8'

while line = STDIN.gets
  a = line.strip.chomp.split "\t"
  a[3..a.size].each { |r|
    id = r.split("|||")[0..2].join("|||").to_s.strip.gsub("\s", "_")
    puts "#{id}\t1"
  }
end

