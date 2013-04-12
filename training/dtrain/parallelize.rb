#!/usr/bin/env ruby

require 'trollop'

def usage
  STDERR.write "Usage: "
  STDERR.write "ruby parallelize.rb -c <dtrain.ini> [-e <epochs=10>] [--randomize/-z] [--reshard/-y] -s <#shards|0> [-p <at once=9999>] -i <input> -r <refs> [--qsub/-q] [--dtrain_binary <path to dtrain binary>] [-l \"l2 select_k 100000\"]\n"
  exit 1
end

opts = Trollop::options do
  opt :config, "dtrain config file", :type => :string
  opt :epochs, "number of epochs", :type => :int, :default => 10
  opt :lplp_args, "arguments for lplp.rb", :type => :string, :default => "l2 select_k 100000"
  opt :randomize, "randomize shards before each epoch", :type => :bool, :short => '-z', :default => false
  opt :reshard, "reshard after each epoch", :type => :bool, :short => '-y', :default => false
  opt :shards, "number of shards", :type => :int
  opt :processes_at_once, "have this number (max) running at the same time", :type => :int, :default => 9999
  opt :input, "input", :type => :string
  opt :references, "references", :type => :string
  opt :qsub, "use qsub", :type => :bool, :default => false
  opt :dtrain_binary, "path to dtrain binary", :type => :string
end
usage if not opts[:config]&&opts[:shards]&&opts[:input]&&opts[:references]


dtrain_dir = File.expand_path File.dirname(__FILE__)
if not opts[:dtrain_binary]
  dtrain_bin = "#{dtrain_dir}/dtrain"
else
  dtrain_bin = opts[:dtrain_binary]
end
ruby       = '/usr/bin/ruby'
lplp_rb    = "#{dtrain_dir}/lplp.rb"
lplp_args  = opts[:lplp_args]
cat        = '/bin/cat'

ini        = opts[:config]
epochs     = opts[:epochs]
rand       = opts[:randomize]
reshard    = opts[:reshard]
predefined_shards = false
if opts[:shards] == 0
  predefined_shards = true
  num_shards = 0
else
  num_shards = opts[:shards]
end
input = opts[:input]
refs  = opts[:references]
use_qsub       = opts[:qsub]
shards_at_once = opts[:processes_at_once]

`mkdir work`

def make_shards(input, refs, num_shards, epoch, rand)
  lc = `wc -l #{input}`.split.first.to_i
  index = (0..lc-1).to_a
  index.reverse!
  index.shuffle! if rand
  shard_sz = lc / num_shards
  leftover = lc % num_shards
  in_f = File.new input, 'r'
  in_lines = in_f.readlines
  refs_f = File.new refs, 'r'
  refs_lines = refs_f.readlines
  shard_in_files = []
  shard_refs_files = []
  in_fns = []
  refs_fns = []
  0.upto(num_shards-1) { |shard|
    in_fn = "work/shard.#{shard}.#{epoch}.in"
    shard_in = File.new in_fn, 'w+'
    in_fns << in_fn
    refs_fn = "work/shard.#{shard}.#{epoch}.refs"
    shard_refs = File.new refs_fn, 'w+'
    refs_fns << refs_fn
    0.upto(shard_sz-1) { |i|
      j = index.pop
      shard_in.write in_lines[j]
      shard_refs.write refs_lines[j]
    }
    shard_in_files << shard_in
    shard_refs_files << shard_refs
  }
  while leftover > 0
    j = index.pop
    shard_in_files[-1].write in_lines[j]
    shard_refs_files[-1].write refs_lines[j]
    leftover -= 1
  end
  (shard_in_files + shard_refs_files).each do |f| f.close end
  in_f.close
  refs_f.close
  return [in_fns, refs_fns]
end

input_files = []
refs_files = []
if predefined_shards
  input_files = File.new(input).readlines.map {|i| i.strip }
  refs_files = File.new(refs).readlines.map {|i| i.strip }
  num_shards = input_files.size
else
  input_files, refs_files = make_shards input, refs, num_shards, 0, rand
end

0.upto(epochs-1) { |epoch|
  puts "epoch #{epoch+1}"
  pids = []
  input_weights = ''
  if epoch > 0 then input_weights = "--input_weights work/weights.#{epoch-1}" end
  weights_files = []
  shard = 0
  remaining_shards = num_shards
  while remaining_shards > 0
    shards_at_once.times {
      break if remaining_shards==0
      qsub_str_start = qsub_str_end = ''
      local_end = ''
      if use_qsub
        qsub_str_start = "qsub -cwd -sync y -b y -j y -o work/out.#{shard}.#{epoch} -N dtrain.#{shard}.#{epoch} \""
        qsub_str_end = "\""
        local_end = ''
      else
        local_end = "2>work/out.#{shard}.#{epoch}"
      end
      pids << Kernel.fork {
        `#{qsub_str_start}#{dtrain_bin} -c #{ini}\
          --input #{input_files[shard]}\
          --refs #{refs_files[shard]} #{input_weights}\
          --output work/weights.#{shard}.#{epoch}#{qsub_str_end} #{local_end}`
      }
      weights_files << "work/weights.#{shard}.#{epoch}"
      shard += 1
      remaining_shards -= 1
    }
    pids.each { |pid| Process.wait(pid) }
    pids.clear
  end
  `#{cat} work/weights.*.#{epoch} > work/weights_cat`
  `#{ruby} #{lplp_rb} #{lplp_args} #{num_shards} < work/weights_cat > work/weights.#{epoch}`
  if rand and reshard and epoch+1!=epochs
    input_files, refs_files = make_shards input, refs, num_shards, epoch+1, rand
  end
}

`rm work/weights_cat`

