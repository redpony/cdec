#!/usr/bin/env ruby

require 'trollop'
require 'zipf'

conf = Trollop::options do
  opt :conf,                      "dtrain configuration",                 :type => :string,                                   :short => '-c'
  opt :input,                     "input as bitext (f ||| e)",            :type => :string,                                   :short => '-i'
  opt :epochs,                    "number of epochs",                     :type => :int,    :default => 10,                   :short => '-e'
  opt :randomize,                 "randomize shards once",                :type => :bool,   :default => false,                :short => '-z'
  opt :reshard,                   "randomize after each epoch",           :type => :bool,   :default => false,                :short => '-y'
  opt :shards,                    "number of shards",                     :type => :int,                                      :short => '-s'
  opt :weights,                   "input weights for first epoch",        :type => :string, :default => '',                   :short => '-w'
  opt :lplp_args,                 "arguments for lplp.rb",                :type => :string, :default => "l2 select_k 100000", :short => '-l'
  opt :per_shard_decoder_configs, "give custom decoder config per shard", :type => :string,                                   :short => '-o'
  opt :processes_at_once,         "jobs to run at oce",                   :type => :int,    :default => 9999,                 :short => '-p'
  opt :qsub,                      "use qsub",                             :type => :bool,   :default => false,                :short => '-q'
  opt :qsub_args,                 "extra args for qsub",                  :type => :string, :default => "h_vmem=5G",          :short => '-r'
  opt :dtrain_binary,             "path to dtrain binary",                :type => :string,                                   :short => '-d'
  opt :adadelta,                  "use adadelta",                         :type => :bool,   :default => false,                :short => '-D'
end

dtrain_dir = File.expand_path File.dirname(__FILE__)
if not conf[:dtrain_binary]
  dtrain_bin = "#{dtrain_dir}/dtrain"
else
  dtrain_bin = conf[:dtrain_binary]
end
lplp_rb    = "#{dtrain_dir}/lplp.rb"
lplp_args  = conf[:lplp_args]

dtrain_conf       = conf[:conf]
epochs            = conf[:epochs]
rand              = conf[:randomize]
reshard           = conf[:reshard]
predefined_shards         = false
per_shard_decoder_configs = false
if conf[:shards] == 0
  predefined_shards = true
  num_shards = 0
  per_shard_decoder_configs = true if conf[:per_shard_decoder_configs]
else
  num_shards = conf[:shards]
end
input               = conf[:input]
use_qsub            = conf[:qsub]
shards_at_once      = conf[:processes_at_once]
first_input_weights = conf[:weights]
use_adadelta        = conf[:adadelta]

`mkdir work`

def make_shards input, num_shards, epoch, rand
  lc = `wc -l #{input}`.split.first.to_i
  index = (0..lc-1).to_a
  index.reverse!
  index.shuffle! if rand
  shard_sz = (lc / num_shards.to_f).round 0
  leftover = lc - (num_shards*shard_sz)
  leftover = [0, leftover].max
  in_f = File.new input, 'r'
  in_lines = in_f.readlines
  shard_in_files = []
  in_fns = []
  real_num_shards = 0
  0.upto(num_shards-1) { |shard|
    break if index.size==0
    real_num_shards += 1
    in_fn = "work/shard.#{shard}.#{epoch}.gz"
    shard_in = WriteFile.new in_fn
    in_fns << in_fn
    0.upto(shard_sz-1) { |i|
      j = index.pop
      break if !j
      shard_in.write in_lines[j]
    }
    shard_in_files << shard_in
  }
  while leftover > 0
    j = index.pop
    break if !j
    shard_in_files[-1].write in_lines[j]
    leftover -= 1
  end
  shard_in_files.each do |f| f.close end
  in_f.close
  return in_fns, real_num_shards
end

input_files = []
if predefined_shards
  input_files = File.new(input).readlines.map { |i| i.strip }
  if per_shard_decoder_configs
    decoder_configs = ReadFile.readlines_strip(conf[:per_shard_decoder_configs]
                                              ).map { |i| i.strip }
  end
  num_shards = input_files.size
else
  input_files, num_shards = make_shards input, num_shards, 0, rand
end

0.upto(epochs-1) { |epoch|
  puts "epoch #{epoch+1}"
  pids = []
  input_weights = ''
  input_weights = "--input_weights work/weights.#{epoch-1}.gz" if epoch>0
  shard = 0
  remaining_shards = num_shards
  while remaining_shards > 0
    shards_at_once.times {
      break if remaining_shards==0
      qsub_str_start = qsub_str_end = local_end = ''
      if use_qsub
        qsub_str_start = "qsub -l #{conf[:qsub_args]} -cwd -sync y -b y -j y\
                           -o work/out.#{shard}.#{epoch}\
                           -N dtrain.#{shard}.#{epoch} \""
        qsub_str_end = "\""
        local_end = ''
      else
        local_end = "2>work/out.#{shard}.#{epoch}"
      end
      if per_shard_decoder_configs
        cdec_conf = "--decoder_conf #{decoder_configs[shard]}"
      else
        cdec_conf = ""
      end
      adadelta_input = ""
      adadelta_output = ""
      if use_adadelta
        adadelta_output = "--adadelta_output work/adadelta.#{shard}.#{epoch}"
        if epoch > 0
          adadelta_input = "--adadelta_input work/adadelta.#{epoch-1}"
        end
      end
      if first_input_weights != '' && epoch == 0
        input_weights = "--input_weights #{first_input_weights}"
      end
      pids << Kernel.fork {
        `#{qsub_str_start}#{dtrain_bin} -c #{dtrain_conf} #{cdec_conf}\
          #{input_weights}\
          #{adadelta_output} #{adadelta_input}\
          --bitext #{input_files[shard]}\
          --output work/weights.#{shard}.#{epoch}.gz#{qsub_str_end} #{local_end}`
      }
      shard += 1
      remaining_shards -= 1
    }
    pids.each { |pid| Process.wait(pid) }
    pids.clear
  end
  `zcat work/weights.*.#{epoch}.gz \
  | ruby #{lplp_rb} #{lplp_args} #{num_shards} \
  | gzip -c \
  > work/weights.#{epoch}.gz`
  if use_adadelta
    h = {}
    ReadFile.readlines_strip("work/weights.#{epoch}.gz").map { |line|
      h[line.split.first] = true
    }
    max = (2**(0.size * 8 -2) -1)
    ["gradient", "update"].each { |i|
      `zcat work/adadelta.*.#{epoch}.#{i}.gz \
      | ruby #{lplp_rb} l0 select_k #{max} #{num_shards} \
      | gzip -c \
      > work/adadelta_avg.#{i}.gz`
      o = WriteFile.new "work/adadelta.#{epoch}.#{i}.gz"
      ReadFile.readlines_strip("work/adadelta_avg.#{i}.gz").each { |line|
        k,v = line.split
        if h.has_key? k
          o.write "#{k} #{v}\n"
        end
      }
      `rm work/adadelta_avg.#{i}.gz`
      o.close
    }
  end
  if rand and reshard and epoch+1!=epochs
    input_files, num_shards = make_shards input, num_shards, epoch+1, rand
  end
}

