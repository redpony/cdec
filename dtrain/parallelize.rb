#!/usr/bin/env ruby


if ARGV.size != 5
  STDERR.write "Usage: "
  STDERR.write "ruby parallelize.rb <#shards> <input> <refs> <epochs> <dtrain.ini>\n"
  exit
end

dtrain_bin = '/home/pks/bin/dtrain_local'
ruby       = '/usr/bin/ruby'
lplp_rb    = '/home/pks/mt/cdec-dtrain/dtrain/hstreaming/lplp.rb'
lplp_args  = 'l2 select_k 100000'
gzip       = '/bin/gzip'

num_shards = ARGV[0].to_i
input      = ARGV[1]
refs       = ARGV[2]
epochs     = ARGV[3].to_i
ini        = ARGV[4]


`mkdir work`

def make_shards(input, refs, num_shards)
  lc = `wc -l #{input}`.split.first.to_i
  shard_sz = lc / num_shards
  leftover = lc % num_shards
  in_f = File.new input, 'r'
  refs_f = File.new refs, 'r'
  shard_in_files = []
  shard_refs_files = []
  0.upto(num_shards-1) { |shard|
    shard_in = File.new "work/shard.#{shard}.in", 'w+'
    shard_refs = File.new "work/shard.#{shard}.refs", 'w+'
    0.upto(shard_sz-1) { |i|
      shard_in.write in_f.gets
      shard_refs.write refs_f.gets
    }
    shard_in_files << shard_in
    shard_refs_files << shard_refs
  }
  while leftover > 0
    shard_in_files[-1].write in_f.gets
    shard_refs_files[-1].write refs_f.gets
    leftover -= 1
  end
  (shard_in_files + shard_refs_files).each do |f| f.close end
  in_f.close
  refs_f.close
end

make_shards input, refs, num_shards

0.upto(epochs-1) { |epoch|
  pids = []
  input_weights = ''
  if epoch > 0 then input_weights = "--input_weights work/weights.#{epoch-1}" end
  weights_files = []
  0.upto(num_shards-1) { |shard|
    pids << Kernel.fork {
      `#{dtrain_bin} -c #{ini}\
        --input work/shard.#{shard}.in\
        --refs work/shard.#{shard}.refs #{input_weights}\
        --output work/weights.#{shard}.#{epoch}\
        &> work/out.#{shard}.#{epoch}`
    }
    weights_files << "work/weights.#{shard}.#{epoch}"
  }
  pids.each { |pid| Process.wait(pid) }
  cat = File.new('work/weights_cat', 'w+')
  weights_files.each { |f| cat.write File.new(f, 'r').read }
  cat.close
  `#{ruby} #{lplp_rb} #{lplp_args} #{num_shards} < work/weights_cat &> work/weights.#{epoch}`
}

`rm work/weights_cat`
`#{gzip} work/*`

