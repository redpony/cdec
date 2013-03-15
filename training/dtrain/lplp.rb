# lplp.rb

# norms
def l0(feature_column, n)
  if feature_column.size >= n then return 1 else return 0 end
end

def l1(feature_column, n=-1)
  return feature_column.map { |i| i.abs }.reduce { |sum,i| sum+i }
end

def l2(feature_column, n=-1)
  return Math.sqrt feature_column.map { |i| i.abs2 }.reduce { |sum,i| sum+i }
end

def linfty(feature_column, n=-1)
  return feature_column.map { |i| i.abs }.max
end

# stats
def median(feature_column, n)
  return feature_column.concat(0.step(n-feature_column.size-1).map{|i|0}).sort[feature_column.size/2]
end

def mean(feature_column, n)
  return feature_column.reduce { |sum, i| sum+i } / n
end

# selection
def select_k(weights, norm_fun, n, k=10000)
  weights.sort{|a,b| norm_fun.call(b[1], n) <=> norm_fun.call(a[1], n)}.each { |p|
    puts "#{p[0]}\t#{mean(p[1], n)}"
    k -= 1
    if k == 0 then break end
  }
end

def cut(weights, norm_fun, n, epsilon=0.0001)
  weights.each { |k,v|
    if norm_fun.call(v, n).abs >= epsilon
      puts "#{k}\t#{mean(v, n)}"
    end
  }
end

# test
def _test()
  puts
  w = {}
  w["a"] = [1, 2, 3]
  w["b"] = [1, 2]
  w["c"] = [66]
  w["d"] = [10, 20, 30]
  n = 3
  puts w.to_s
  puts
  puts "select_k"
  puts "l0 expect ad"
  select_k(w, method(:l0), n, 2)
  puts "l1 expect cd"
  select_k(w, method(:l1), n, 2)
  puts "l2 expect c"
  select_k(w, method(:l2), n, 1)
  puts
  puts "cut"
  puts "l1 expect cd"
  cut(w, method(:l1), n, 7)
  puts
  puts "median"
  a = [1,2,3,4,5]
  puts a.to_s
  puts median(a, 5)
  puts
  puts "#{median(a, 7)} <- that's because we add missing 0s:"
  puts a.concat(0.step(7-a.size-1).map{|i|0}).to_s
  puts
  puts "mean expect bc"
  w.clear
  w["a"] = [2]
  w["b"] = [2.1]
  w["c"] = [2.2]
  cut(w, method(:mean), 1, 2.05)
 exit
end
#_test()

# actually do something
def usage()
  puts "lplp.rb <l0,l1,l2,linfty,mean,median> <cut|select_k> <k|threshold> [n] < <input>"
  puts "   l0...: norms for selection"
  puts "select_k: only output top k (according to the norm of their column vector) features"
  puts "     cut: output features with weight >= threshold"
  puts "       n: if we do not have a shard count use this number for averaging"
  exit
end

if ARGV.size < 3 then usage end
norm_fun = method(ARGV[0].to_sym)
type = ARGV[1]
x = ARGV[2].to_f

shard_count_key = "__SHARD_COUNT__"

STDIN.set_encoding 'utf-8'
STDOUT.set_encoding 'utf-8'

w = {}
shard_count = 0
while line = STDIN.gets
  key, val = line.split /\s+/
  if key == shard_count_key
    shard_count += 1
    next
  end
  if w.has_key? key
    w[key].push val.to_f
  else
    w[key] = [val.to_f]
  end
end

if ARGV.size == 4 then shard_count = ARGV[3].to_f end

if type == 'cut'
  cut(w, norm_fun, shard_count, x)
elsif type == 'select_k'
  select_k(w, norm_fun, shard_count, x)
else
  puts "oh oh"
end

