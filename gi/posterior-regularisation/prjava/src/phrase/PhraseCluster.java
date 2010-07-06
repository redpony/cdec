package phrase;

import io.FileUtil;
import java.io.PrintStream;
import java.util.Arrays;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.LinkedBlockingQueue;

public class PhraseCluster {
	
	public int K;
	public double scale;
	private int n_phrase;
	private int n_words;
	public PhraseCorpus c;
	private ExecutorService pool; 
	
	/**@brief
	 * emit[tag][position][word]
	 */
	private double emit[][][];
	private double pi[][];

	
	public static void main(String[] args) {
		String input_fname = args[0];
		int tags = Integer.parseInt(args[1]);
		String outputDir = args[2];
		int iterations = Integer.parseInt(args[3]);
		double scale = Double.parseDouble(args[4]);
		int threads = Integer.parseInt(args[5]);
		
		PhraseCorpus corpus = new PhraseCorpus(input_fname);
		PhraseCluster cluster = new PhraseCluster(tags, corpus, scale, threads);
		
		PhraseObjective.ps = FileUtil.openOutFile(outputDir + "/phrase_stat.out");
		
		for(int i=0;i<iterations;i++){
			double o = cluster.PREM();
		    //double o = cluster.EM();
			PhraseObjective.ps.println("ITER: "+i+" objective: " + o);
		}
		
		PrintStream ps=io.FileUtil.openOutFile(outputDir + "/posterior.out");
		cluster.displayPosterior(ps);
		ps.println();
		cluster.displayModelParam(ps);
		ps.close();
		PhraseObjective.ps.close();
		
		cluster.finish();
	}

	public PhraseCluster(int numCluster, PhraseCorpus corpus, double scale, int threads){
		K=numCluster;
		c=corpus;
		n_words=c.wordLex.size();
		n_phrase=c.data.length;
		this.scale = scale;
		if (threads > 0)
			pool = Executors.newFixedThreadPool(threads);
		
		emit=new double [K][PhraseCorpus.NUM_CONTEXT][n_words];
		pi=new double[n_phrase][K];
		
		for(double [][]i:emit){
			for(double []j:i){
				arr.F.randomise(j);
			}
		}
		
		for(double []j:pi){
			arr.F.randomise(j);
		}
	}
	
	public void finish()
	{
		if (pool != null)
			pool.shutdown();
	}
		
	public double EM(){
		double [][][]exp_emit=new double [K][PhraseCorpus.NUM_CONTEXT][n_words];
		double [][]exp_pi=new double[n_phrase][K];
		
		double loglikelihood=0;
		
		//E
		for(int phrase=0;phrase<c.data.length;phrase++){
			int [][] data=c.data[phrase];
			for(int ctx=0;ctx<data.length;ctx++){
				int context[]=data[ctx];
				double p[]=posterior(phrase,context);
				loglikelihood+=Math.log(arr.F.l1norm(p));
				arr.F.l1normalize(p);
				
				int contextCnt=context[context.length-1];
				//increment expected count
				for(int tag=0;tag<K;tag++){
					for(int pos=0;pos<context.length-1;pos++){
						exp_emit[tag][pos][context[pos]]+=p[tag]*contextCnt;
					}
					
					exp_pi[phrase][tag]+=p[tag]*contextCnt;
				}
			}
		}
		
		System.out.println("Log likelihood: "+loglikelihood);
		
		//M
		for(double [][]i:exp_emit){
			for(double []j:i){
				arr.F.l1normalize(j);
			}
		}
		
		emit=exp_emit;
		
		for(double []j:exp_pi){
			arr.F.l1normalize(j);
		}
		
		pi=exp_pi;
		
		return loglikelihood;
	}
	
	public double PREM(){
		if (pool != null)
			return PREMParallel();
		
		double [][][]exp_emit=new double [K][PhraseCorpus.NUM_CONTEXT][n_words];
		double [][]exp_pi=new double[n_phrase][K];
		
		double loglikelihood=0;
		double primal=0;
		//E
		for(int phrase=0;phrase<c.data.length;phrase++){
			PhraseObjective po=new PhraseObjective(this,phrase);
			po.optimizeWithProjectedGradientDescent();
			double [][] q=po.posterior();
			loglikelihood+=po.getValue();
			primal+=po.primal();
			for(int edge=0;edge<q.length;edge++){
				int []context=c.data[phrase][edge];
				int contextCnt=context[context.length-1];
				//increment expected count
				for(int tag=0;tag<K;tag++){
					for(int pos=0;pos<context.length-1;pos++){
						exp_emit[tag][pos][context[pos]]+=q[edge][tag]*contextCnt;
					}
					
					exp_pi[phrase][tag]+=q[edge][tag]*contextCnt;
				}
			}
		}
		
		System.out.println("Log likelihood: "+loglikelihood);
		System.out.println("Primal Objective: "+primal);
		
		//M
		for(double [][]i:exp_emit){
			for(double []j:i){
				arr.F.l1normalize(j);
			}
		}
		
		emit=exp_emit;
		
		for(double []j:exp_pi){
			arr.F.l1normalize(j);
		}
		
		pi=exp_pi;
		
		return primal;
	}

	public double PREMParallel(){
		assert(pool != null);
		final LinkedBlockingQueue<PhraseObjective> expectations 
			= new LinkedBlockingQueue<PhraseObjective>();
		
		double [][][]exp_emit=new double [K][PhraseCorpus.NUM_CONTEXT][n_words];
		double [][]exp_pi=new double[n_phrase][K];
		
		double loglikelihood=0;
		double primal=0;
		//E
		for(int phrase=0;phrase<c.data.length;phrase++){
			final int p=phrase;
			pool.execute(new Runnable() {
				public void run() {
					try {
						//System.out.println("" + Thread.currentThread().getId() + " optimising lambda for " + p);
						PhraseObjective po = new PhraseObjective(PhraseCluster.this, p);
						po.optimizeWithProjectedGradientDescent();
						//System.out.println("" + Thread.currentThread().getId() + " done optimising lambda for " + p);
						expectations.put(po);
						//System.out.println("" + Thread.currentThread().getId() + " added to queue " + p);
					} catch (InterruptedException e) {
						System.err.println(Thread.currentThread().getId() + " Local e-step thread interrupted; will cause deadlock.");
						e.printStackTrace();
					}
				}
			});
		}
		
		// aggregate the expectations as they become available
		for(int count=0;count<c.data.length;count++) {
			try {
				//System.out.println("" + Thread.currentThread().getId() + " reading queue #" + count);

				// wait (blocking) until something is ready
				PhraseObjective po = expectations.take();
				// process
				int phrase = po.phrase;
				//System.out.println("" + Thread.currentThread().getId() + " taken phrase " + phrase);
				double [][] q=po.posterior();
				loglikelihood+=po.getValue();
				primal+=po.primal();
				for(int edge=0;edge<q.length;edge++){
					int []context=c.data[phrase][edge];
					int contextCnt=context[context.length-1];
					//increment expected count
					for(int tag=0;tag<K;tag++){
						for(int pos=0;pos<context.length-1;pos++){
							exp_emit[tag][pos][context[pos]]+=q[edge][tag]*contextCnt;
						}
						exp_pi[phrase][tag]+=q[edge][tag]*contextCnt;
					}
				}
			} catch (InterruptedException e){
				System.err.println("M-step thread interrupted. Probably fatal!");
				e.printStackTrace();
			}
		}
		
		System.out.println("Log likelihood: "+loglikelihood);
		System.out.println("Primal Objective: "+primal);
		
		//M
		for(double [][]i:exp_emit){
			for(double []j:i){
				arr.F.l1normalize(j);
			}
		}
		
		emit=exp_emit;
		
		for(double []j:exp_pi){
			arr.F.l1normalize(j);
		}
		
		pi=exp_pi;
		
		return primal;
	}
	
	/**
	 * 
	 * @param phrase index of phrase
	 * @param ctx array of context
	 * @return unnormalized posterior
	 */
	public double[]posterior(int phrase, int[]ctx){
		double[] prob=Arrays.copyOf(pi[phrase], K);
		
		for(int tag=0;tag<K;tag++){
			for(int c=0;c<ctx.length-1;c++){
				int word=ctx[c];
				prob[tag]*=emit[tag][c][word];
			}
		}
		
		return prob;
	}
	
	public void displayPosterior(PrintStream ps)
	{
		
		c.buildList();
		
		for (int i = 0; i < n_phrase; ++i)
		{
			int [][]data=c.data[i];
			for (int[] e: data)
			{
				double probs[] = posterior(i, e);
				arr.F.l1normalize(probs);

				// emit phrase
				ps.print(c.phraseList[i]);
				ps.print("\t");
				ps.print(c.getContextString(e));
				ps.print("||| C=" + e[e.length-1] + " |||");

				int t=arr.F.argmax(probs);
				
				ps.print(t+"||| [");
				for(t=0;t<K;t++){
					ps.print(probs[t]+", ");
				}
				// for (int t = 0; t < numTags; ++t)
				// System.out.print(" " + probs[t]);
				ps.println("]");
			}
		}
	}
	
	public void displayModelParam(PrintStream ps)
	{
		
		c.buildList();
		
		ps.println("P(tag|phrase)");
		for (int i = 0; i < n_phrase; ++i)
		{
			ps.print(c.phraseList[i]);
			for(int j=0;j<pi[i].length;j++){
				ps.print("\t"+pi[i][j]);
			}
			ps.println();
		}
		
		ps.println("P(word|tag,position)");
		for (int i = 0; i < K; ++i)
		{
			ps.println(i);
			for(int position=0;position<PhraseCorpus.NUM_CONTEXT;position++){
				ps.println(position);
				for(int word=0;word<emit[i][position].length;word++){
					if((word+1)%100==0){
						ps.println();
					}
					ps.print(c.wordList[word]+"="+emit[i][position][word]+"\t");
				}
				ps.println();
			}
			ps.println();
		}
		
	}
}
