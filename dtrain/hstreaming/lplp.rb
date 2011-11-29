# lplp.rb

# norms
def l0(feature_column, n)
  if feature_column.size == n then return 1 else return 0 end
end

def l1(feature_column, n=-1)
  return feature_column.reduce { |sum, i| i.abs }
end

def l2(feature_column, n=-1)
  return Math.sqrt feature_column.reduce { |sum, i| i**2 }
end

def linfty(feature_column, n=-1)
  return feature_column.map { |i| i.abs }.max
end

# stats
def M(feature_column, n)
  return feature_column.concat(0.step(n-feature_column.size-1).map{|i|0}).sort[feature_column.size/2]
end

def mean(feature_column, n)
  return feature_column.reduce { |sum, i| sum+i } / n
end

# selection
def select_k(weights, normfn, n, k=10000)
  weights.sort{|a,b| normfn.call(b[1], n) <=> normfn.call(a[1], n)}.each { |p|
    puts "#{p[0]}\t#{mean(p[1], n)}" 
    k -= 1
    if k == 0 then break end
  }
end

def cut(weights, normfn, n, epsilon=0.0001)
  weights.each { |k,v|
    if normfn.call(v).abs > epsilon
      puts "#{k}\t#{mean(v, n)}"
    end
  }
end


shard_count_key = "__SHARD_COUNT__"

STDIN.set_encoding 'utf-8'
STDOUT.set_encoding 'utf-8'

w = {}
shard_count = 0
while line = STDIN.gets
  key, val = line.split /\t/
  if k = shard_count_key
    shard_count += 1
    next
  end
  if w.has_key? key
    w[key].push val
  else
    w[key] = [val]
  end
end

select_k(w, method(:l1), shard_count, 100000)

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
puts "l1 expect c"
select_k(w, method(:l1), n, 1)
puts "l2 expect d"
select_k(w, method(:l2), n, 1)
puts
puts "cut"
puts "l1 expect cd"
cut(w, method(:l1), n, 7)
puts
puts "M"
a = [1,3,4,5,6]
puts a.to_s
puts M(a, 7)
puts "that's because we add missing 0s"
puts a.concat(0.step(7-a.size-1).map{|i|0}).to_s
puts
end

#_test()

