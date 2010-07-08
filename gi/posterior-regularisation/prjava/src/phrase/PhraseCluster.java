package phrase;

import gnu.trove.TIntArrayList;
import io.FileUtil;
import java.io.IOException;
import java.io.PrintStream;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.LinkedBlockingQueue;

import phrase.Corpus.Edge;

public class PhraseCluster {
	
	public int K;
	public double scalePT, scaleCT;
	private int n_phrases, n_words, n_contexts, n_positions;
	public Corpus c;
	private ExecutorService pool; 
	
	// emit[tag][position][word] = p(word | tag, position in context)
	private double emit[][][];
	// pi[phrase][tag] = p(tag | phrase)
	private double pi[][];
	
	public static void main(String[] args) 
	{
		String input_fname = args[0];
		int tags = Integer.parseInt(args[1]);
		String output_fname = args[2];
		int iterations = Integer.parseInt(args[3]);
		double scalePT = Double.parseDouble(args[4]);
		double scaleCT = Double.parseDouble(args[5]);
		int threads = Integer.parseInt(args[6]);
		boolean runEM = Boolean.parseBoolean(args[7]);
		
		assert(tags >= 2);
		assert(scalePT >= 0);
		assert(scaleCT >= 0);
		
		Corpus corpus = null;
		try {
			corpus = Corpus.readFromFile(FileUtil.openBufferedReader(input_fname));
		} catch (IOException e) {
			System.err.println("Failed to open input file: " + input_fname);
			e.printStackTrace();
			System.exit(1);
		}
		PhraseCluster cluster = new PhraseCluster(tags, corpus, scalePT, scaleCT, threads);
		
		//PhraseObjective.ps = FileUtil.openOutFile(outputDir + "/phrase_stat.out");
		
		double last = 0;
		for(int i=0;i<iterations;i++){
			
			double o;
			if (runEM || i < 3) 
				o = cluster.EM();
			else if (scaleCT == 0)
			{
				if (threads >= 1)
					o = cluster.PREM_phrase_constraints_parallel();
				else
					o = cluster.PREM_phrase_constraints();
			}
			else 
				o = cluster.PREM_phrase_context_constraints();
			
			//PhraseObjective.ps.
			System.out.println("ITER: "+i+" objective: " + o);
			last = o;
		}
		
		double pl1lmax = cluster.phrase_l1lmax();
		double cl1lmax = cluster.context_l1lmax();
		System.out.println("Final posterior phrase l1lmax " + pl1lmax + " context l1lmax " + cl1lmax);
		if (runEM) System.out.println("With PR objective " + (last - scalePT*pl1lmax - scaleCT*cl1lmax));
		
		PrintStream ps=io.FileUtil.openOutFile(output_fname);
		cluster.displayPosterior(ps);
		ps.close();
		
		//PhraseObjective.ps.close();

		//ps = io.FileUtil.openOutFile(outputDir + "/parameters.out");
		//cluster.displayModelParam(ps);
		//ps.close();
		
		cluster.finish();
	}

	public PhraseCluster(int numCluster, Corpus corpus, double scalep, double scalec, int threads){
		K=numCluster;
		c=corpus;
		n_words=c.getNumWords();
		n_phrases=c.getNumPhrases();
		n_contexts=c.getNumContexts();
		n_positions=c.getNumContextPositions();
		this.scalePT = scalep;
		this.scaleCT = scalec;
		if (threads > 0 && scalec <= 0)
			pool = Executors.newFixedThreadPool(threads);
		
		emit=new double [K][n_positions][n_words];
		pi=new double[n_phrases][K];
		
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
		double [][][]exp_emit=new double [K][n_positions][n_words];
		double [][]exp_pi=new double[n_phrases][K];
		
		double loglikelihood=0;
		
		//E
		for(int phrase=0; phrase < n_phrases; phrase++){
			List<Edge> contexts = c.getEdgesForPhrase(phrase);

			for (int ctx=0; ctx<contexts.size(); ctx++){
				Edge edge = contexts.get(ctx);
				double p[]=posterior(edge);
				double z = arr.F.l1norm(p);
				assert z > 0;
				loglikelihood+=Math.log(z);
				arr.F.l1normalize(p);
				
				int count = edge.getCount();
				//increment expected count
				TIntArrayList context = edge.getContext();
				for(int tag=0;tag<K;tag++){
					for(int pos=0;pos<n_positions;pos++){
						exp_emit[tag][pos][context.get(pos)]+=p[tag]*count;
					}
					
					exp_pi[phrase][tag]+=p[tag]*count;
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
	
	public double PREM_phrase_constraints(){
		assert (scaleCT <= 0);
		
		double [][][]exp_emit=new double[K][n_positions][n_words];
		double [][]exp_pi=new double[n_phrases][K];
		
		double loglikelihood=0, kl=0, l1lmax=0, primal=0;
		//E
		for(int phrase=0; phrase<n_phrases; phrase++){
			PhraseObjective po=new PhraseObjective(this,phrase);
			po.optimizeWithProjectedGradientDescent();
			double [][] q=po.posterior();
			loglikelihood += po.loglikelihood();
			kl += po.KL_divergence();
			l1lmax += po.l1lmax();
			primal += po.primal();
			List<Edge> edges = c.getEdgesForPhrase(phrase);

			for(int edge=0;edge<q.length;edge++){
				Edge e = edges.get(edge);
				TIntArrayList context = e.getContext();
				int contextCnt = e.getCount();
				//increment expected count
				for(int tag=0;tag<K;tag++){
					for(int pos=0;pos<n_positions;pos++){
						exp_emit[tag][pos][context.get(pos)]+=q[edge][tag]*contextCnt;
					}
					
					exp_pi[phrase][tag]+=q[edge][tag]*contextCnt;
				}
			}
		}
		
		System.out.println("\tllh:            " + loglikelihood);
		System.out.println("\tKL:             " + kl);
		System.out.println("\tphrase l1lmax:  " + l1lmax);
		
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

	public double PREM_phrase_constraints_parallel()
	{
		assert(pool != null);
		assert(scaleCT <= 0);
		
		final LinkedBlockingQueue<PhraseObjective> expectations 
			= new LinkedBlockingQueue<PhraseObjective>();
		
		double [][][]exp_emit=new double [K][n_positions][n_words];
		double [][]exp_pi=new double[n_phrases][K];
		
		double loglikelihood=0, kl=0, l1lmax=0, primal=0;

		//E
		for(int phrase=0;phrase<n_phrases;phrase++){
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
		for(int count=0;count<n_phrases;count++) {
			try {
				//System.out.println("" + Thread.currentThread().getId() + " reading queue #" + count);

				// wait (blocking) until something is ready
				PhraseObjective po = expectations.take();
				// process
				int phrase = po.phrase;
				//System.out.println("" + Thread.currentThread().getId() + " taken phrase " + phrase);
				double [][] q=po.posterior();
				loglikelihood += po.loglikelihood();
				kl += po.KL_divergence();
				l1lmax += po.l1lmax();
				primal += po.primal();
				
				List<Edge> edges = c.getEdgesForPhrase(phrase);
				for(int edge=0;edge<q.length;edge++){
					Edge e = edges.get(edge);
					TIntArrayList context = e.getContext();
					int contextCnt = e.getCount();
					//increment expected count
					for(int tag=0;tag<K;tag++){
						for(int pos=0;pos<n_positions;pos++){
							exp_emit[tag][pos][context.get(pos)]+=q[edge][tag]*contextCnt;
						}
						exp_pi[phrase][tag]+=q[edge][tag]*contextCnt;
					}
				}
			} catch (InterruptedException e)
			{
				System.err.println("M-step thread interrupted. Probably fatal!");
				e.printStackTrace();
			}
		}
		
		System.out.println("\tllh:            " + loglikelihood);
		System.out.println("\tKL:             " + kl);
		System.out.println("\tphrase l1lmax:  " + l1lmax);
		
		//M
		for(double [][]i:exp_emit)
			for(double []j:i)
				arr.F.l1normalize(j);
		emit=exp_emit;
		
		for(double []j:exp_pi)
			arr.F.l1normalize(j);
		pi=exp_pi;
		
		return primal;
	}

	public double PREM_phrase_context_constraints(){
		assert (scaleCT > 0);
		
		double [][][]exp_emit=new double [K][n_positions][n_words];
		double [][]exp_pi=new double[n_phrases][K];

		//E step
		// TODO: cache the lambda values (the null below)
		PhraseContextObjective pco = new PhraseContextObjective(this, null);
		pco.optimizeWithProjectedGradientDescent();

		//now extract expectations
		List<Corpus.Edge> edges = c.getEdges();
		for(int e = 0; e < edges.size(); ++e)
		{
			double [] q = pco.posterior(e);
			Corpus.Edge edge = edges.get(e);

			TIntArrayList context = edge.getContext();
			int contextCnt = edge.getCount();
			//increment expected count
			for(int tag=0;tag<K;tag++)
			{
				for(int pos=0;pos<n_positions;pos++)
					exp_emit[tag][pos][context.get(pos)]+=q[tag]*contextCnt;
				exp_pi[edge.getPhraseId()][tag]+=q[tag]*contextCnt;
			}
		}
		
		System.out.println("\tllh:            " + pco.loglikelihood());
		System.out.println("\tKL:             " + pco.KL_divergence());
		System.out.println("\tphrase l1lmax:  " + pco.phrase_l1lmax());
		System.out.println("\tcontext l1lmax: " + pco.context_l1lmax());
		
		//M step
		for(double [][]i:exp_emit)
			for(double []j:i)
				arr.F.l1normalize(j);
		emit=exp_emit;
		
		for(double []j:exp_pi)
			arr.F.l1normalize(j);
		pi=exp_pi;
		
		return pco.primal();
	}	
		
	/**
	 * @param phrase index of phrase
	 * @param ctx array of context
	 * @return unnormalized posterior
	 */
	public double[] posterior(Corpus.Edge edge) 
	{
		double[] prob=Arrays.copyOf(pi[edge.getPhraseId()], K);
		
		TIntArrayList ctx = edge.getContext();
		for(int tag=0;tag<K;tag++)
			for(int c=0;c<n_positions;c++)
				prob[tag]*=emit[tag][c][ctx.get(c)];
		
		return prob;
	}
	
	public void displayPosterior(PrintStream ps)
	{	
		for (Edge edge : c.getEdges())
		{
			double probs[] = posterior(edge);
			arr.F.l1normalize(probs);

			// emit phrase
			ps.print(edge.getPhraseString());
			ps.print("\t");
			ps.print(edge.getContextString(true));
			int t=arr.F.argmax(probs);
			ps.println(" ||| C=" + t);
		}
	}
	
	public void displayModelParam(PrintStream ps)
	{
		final double EPS = 1e-6;
		
		ps.println("P(tag|phrase)");
		for (int i = 0; i < n_phrases; ++i)
		{
			ps.print(c.getPhrase(i));
			for(int j=0;j<pi[i].length;j++){
				if (pi[i][j] > EPS)
					ps.print("\t" + j + ": " + pi[i][j]);
			}
			ps.println();
		}
		
		ps.println("P(word|tag,position)");
		for (int i = 0; i < K; ++i)
		{
			for(int position=0;position<n_positions;position++){
				ps.println("tag " + i + " position " + position);
				for(int word=0;word<emit[i][position].length;word++){
					if (emit[i][position][word] > EPS)
						ps.print(c.getWord(word)+"="+emit[i][position][word]+"\t");
				}
				ps.println();
			}
			ps.println();
		}
		
	}
	
	double phrase_l1lmax()
	{
		double sum=0;
		for(int phrase=0; phrase<n_phrases; phrase++)
		{
			double [] maxes = new double[K];
			for (Edge edge : c.getEdgesForPhrase(phrase))
			{
				double p[] = posterior(edge);
				arr.F.l1normalize(p);
				for(int tag=0;tag<K;tag++)
					maxes[tag] = Math.max(maxes[tag], p[tag]);
			}
			for(int tag=0;tag<K;tag++)
				sum += maxes[tag];
		}
		return sum;
	}

	double context_l1lmax()
	{
		double sum=0;
		for(int context=0; context<n_contexts; context++)
		{
			double [] maxes = new double[K];
			for (Edge edge : c.getEdgesForContext(context))
			{
				double p[] = posterior(edge);
				arr.F.l1normalize(p);
				for(int tag=0;tag<K;tag++)
					maxes[tag] = Math.max(maxes[tag], p[tag]);
			}
			for(int tag=0;tag<K;tag++)
				sum += maxes[tag];
		}
		return sum;
	}
}
