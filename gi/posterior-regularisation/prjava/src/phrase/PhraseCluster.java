package phrase;

import gnu.trove.TIntArrayList;
import org.apache.commons.math.special.Gamma;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.PrintStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicLong;
import java.util.regex.Pattern;

import phrase.Corpus.Edge;


public class PhraseCluster {
	
	public int K;
	private int n_phrases, n_words, n_contexts, n_positions;
	public Corpus c;
	public ExecutorService pool; 

	double[] lambdaPTCT;
	double[][] lambdaPT;
	boolean cacheLambda = true;

	// emit[tag][position][word] = p(word | tag, position in context)
	double emit[][][];
	// pi[phrase][tag] = p(tag | phrase)
	double pi[][];
	
	public PhraseCluster(int numCluster, Corpus corpus)
	{
		K=numCluster;
		c=corpus;
		n_words=c.getNumWords();
		n_phrases=c.getNumPhrases();
		n_contexts=c.getNumContexts();
		n_positions=c.getNumContextPositions();

		emit=new double [K][n_positions][n_words];
		pi=new double[n_phrases][K];
		
		for(double [][]i:emit)
			for(double []j:i)
				arr.F.randomise(j, true);

		for(double []j:pi)
			arr.F.randomise(j, true);
	}
	
	void useThreadPool(ExecutorService pool)
	{
		this.pool = pool;
	}

	public double EM(int phraseSizeLimit)
	{
		double [][][]exp_emit=new double [K][n_positions][n_words];
		double []exp_pi=new double[K];
		
		for(double [][]i:exp_emit)
			for(double []j:i)
				Arrays.fill(j, 1e-10);
		
		double loglikelihood=0;
		
		//E
		for(int phrase=0; phrase < n_phrases; phrase++)
		{
			if (phraseSizeLimit >= 1 && c.getPhrase(phrase).size() > phraseSizeLimit)
				continue;

			Arrays.fill(exp_pi, 1e-10);
			
			List<Edge> contexts = c.getEdgesForPhrase(phrase);

			for (int ctx=0; ctx<contexts.size(); ctx++)
			{
				Edge edge = contexts.get(ctx);
				
				double p[]=posterior(edge);
				double z = arr.F.l1norm(p);
				assert z > 0;
				loglikelihood += edge.getCount() * Math.log(z);
				arr.F.l1normalize(p);
				
				double count = edge.getCount();
				//increment expected count
				TIntArrayList context = edge.getContext();
				for(int tag=0;tag<K;tag++)
				{
					for(int pos=0;pos<n_positions;pos++){
						exp_emit[tag][pos][context.get(pos)]+=p[tag]*count;
					}
					exp_pi[tag]+=p[tag]*count;
				}
			}
			arr.F.l1normalize(exp_pi);
			System.arraycopy(exp_pi, 0, pi[phrase], 0, K);
		}

		//M
		for(double [][]i:exp_emit)
			for(double []j:i)
				arr.F.l1normalize(j);
			
		emit=exp_emit;

		return loglikelihood;
	}
	
	public double PREM(double scalePT, double scaleCT, int phraseSizeLimit)
	{
		if (scaleCT == 0)
		{
			if (pool != null)
				return PREM_phrase_constraints_parallel(scalePT, phraseSizeLimit);
			else
				return PREM_phrase_constraints(scalePT, phraseSizeLimit);
		}
		else // FIXME: ignores phraseSizeLimit
			return this.PREM_phrase_context_constraints(scalePT, scaleCT);
	}

	
	public double PREM_phrase_constraints(double scalePT, int phraseSizeLimit)
	{
		double [][][]exp_emit=new double[K][n_positions][n_words];
		double []exp_pi=new double[K];
		
		for(double [][]i:exp_emit)
			for(double []j:i)
				Arrays.fill(j, 1e-10);
		
		if (lambdaPT == null && cacheLambda)
			lambdaPT = new double[n_phrases][];
		
		double loglikelihood=0, kl=0, l1lmax=0, primal=0;
		int failures=0, iterations=0;
		long start = System.currentTimeMillis();
		//E
		for(int phrase=0; phrase<n_phrases; phrase++)
		{
			if (phraseSizeLimit >= 1 && c.getPhrase(phrase).size() > phraseSizeLimit)
			{
				//System.arraycopy(pi[phrase], 0, exp_pi[phrase], 0, K);
				continue;
			}
			
			Arrays.fill(exp_pi, 1e-10);
			
			// FIXME: add rare edge check to phrase objective & posterior processing
			PhraseObjective po = new PhraseObjective(this, phrase, scalePT, (cacheLambda) ? lambdaPT[phrase] : null);
			boolean ok = po.optimizeWithProjectedGradientDescent();
			if (!ok) ++failures;
			if (cacheLambda) lambdaPT[phrase] = po.getParameters();
			iterations += po.getNumberUpdateCalls();
			double [][] q=po.posterior();
			loglikelihood += po.loglikelihood();
			kl += po.KL_divergence();
			l1lmax += po.l1lmax();
			primal += po.primal(scalePT);
			List<Edge> edges = c.getEdgesForPhrase(phrase);

			for(int edge=0;edge<q.length;edge++){
				Edge e = edges.get(edge);
				TIntArrayList context = e.getContext();
				double contextCnt = e.getCount();
				//increment expected count
				for(int tag=0;tag<K;tag++){
					for(int pos=0;pos<n_positions;pos++){
						exp_emit[tag][pos][context.get(pos)]+=q[edge][tag]*contextCnt;
					}
					
					exp_pi[tag]+=q[edge][tag]*contextCnt;
					
				}
			}
			arr.F.l1normalize(exp_pi);
			System.arraycopy(exp_pi, 0, pi[phrase], 0, K);
		}
		
		long end = System.currentTimeMillis();
		if (failures > 0)
			System.out.println("WARNING: failed to converge in " + failures + "/" + n_phrases + " cases");
		System.out.println("\tmean iters:     " + iterations/(double)n_phrases + " elapsed time " + (end - start) / 1000.0);
		System.out.println("\tllh:            " + loglikelihood);
		System.out.println("\tKL:             " + kl);
		System.out.println("\tphrase l1lmax:  " + l1lmax);
		
		//M
		for(double [][]i:exp_emit)
			for(double []j:i)
				arr.F.l1normalize(j);
		emit=exp_emit;
		
		return primal;
	}

	public double PREM_phrase_constraints_parallel(final double scalePT, int phraseSizeLimit)
	{
		assert(pool != null);
		
		final LinkedBlockingQueue<PhraseObjective> expectations 
			= new LinkedBlockingQueue<PhraseObjective>();
		
		double [][][]exp_emit=new double [K][n_positions][n_words];
		double [][]exp_pi=new double[n_phrases][K];
		
		for(double [][]i:exp_emit)
			for(double []j:i)
				Arrays.fill(j, 1e-10);
		for(double []j:exp_pi)
			Arrays.fill(j, 1e-10);
		
		double loglikelihood=0, kl=0, l1lmax=0, primal=0;
		final AtomicInteger failures = new AtomicInteger(0);
		final AtomicLong elapsed = new AtomicLong(0l);
		int iterations=0;
		long start = System.currentTimeMillis();
		List<Future<PhraseObjective>> results = new ArrayList<Future<PhraseObjective>>();
		
		if (lambdaPT == null && cacheLambda)
			lambdaPT = new double[n_phrases][];

		//E
		for(int phrase=0;phrase<n_phrases;phrase++) {
			if (phraseSizeLimit >= 1 && c.getPhrase(phrase).size() > phraseSizeLimit) {
				System.arraycopy(pi[phrase], 0, exp_pi[phrase], 0, K);
				continue;
			}

			final int p=phrase;
			results.add(pool.submit(new Callable<PhraseObjective>() {
				public PhraseObjective call() {
					//System.out.println("" + Thread.currentThread().getId() + " optimising lambda for " + p);
					long start = System.currentTimeMillis();
					PhraseObjective po = new PhraseObjective(PhraseCluster.this, p, scalePT, (cacheLambda) ? lambdaPT[p] : null);
					boolean ok = po.optimizeWithProjectedGradientDescent();
					if (!ok) failures.incrementAndGet();
					long end = System.currentTimeMillis();
					elapsed.addAndGet(end - start);
					//System.out.println("" + Thread.currentThread().getId() + " done optimising lambda for " + p);
					return po;
				}
			}));
		}
		
		// aggregate the expectations as they become available
		for (Future<PhraseObjective> fpo : results)
		{
			try {
				//System.out.println("" + Thread.currentThread().getId() + " reading queue #" + count);

				// wait (blocking) until something is ready
				PhraseObjective po = fpo.get();
				// process
				int phrase = po.phrase;
				if (cacheLambda) lambdaPT[phrase] = po.getParameters();
				//System.out.println("" + Thread.currentThread().getId() + " taken phrase " + phrase);
				double [][] q=po.posterior();
				loglikelihood += po.loglikelihood();
				kl += po.KL_divergence();
				l1lmax += po.l1lmax();
				primal += po.primal(scalePT);
				iterations += po.getNumberUpdateCalls();

				List<Edge> edges = c.getEdgesForPhrase(phrase);
				for(int edge=0;edge<q.length;edge++){
					Edge e = edges.get(edge);
					TIntArrayList context = e.getContext();
					double contextCnt = e.getCount();
					//increment expected count
					for(int tag=0;tag<K;tag++){
						for(int pos=0;pos<n_positions;pos++){
							exp_emit[tag][pos][context.get(pos)]+=q[edge][tag]*contextCnt;
						}
						exp_pi[phrase][tag]+=q[edge][tag]*contextCnt;
					}
				}
			} catch (InterruptedException e) {
				System.err.println("M-step thread interrupted. Probably fatal!");
				throw new RuntimeException(e);
			} catch (ExecutionException e) {
				System.err.println("M-step thread execution died. Probably fatal!");
				throw new RuntimeException(e);
			}
		}
		
		long end = System.currentTimeMillis();
		
		if (failures.get() > 0)
			System.out.println("WARNING: failed to converge in " + failures.get() + "/" + n_phrases + " cases");
		System.out.println("\tmean iters:     " + iterations/(double)n_phrases + " walltime " + (end-start)/1000.0 + " threads " + elapsed.get() / 1000.0);
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
	
	public double PREM_phrase_context_constraints(double scalePT, double scaleCT)
	{	
		double[][][] exp_emit = new double [K][n_positions][n_words];
		double[][] exp_pi = new double[n_phrases][K];

		//E step
		PhraseContextObjective pco = new PhraseContextObjective(this, lambdaPTCT, pool, scalePT, scaleCT);
		boolean ok = pco.optimizeWithProjectedGradientDescent();
		if (cacheLambda) lambdaPTCT = pco.getParameters();

		//now extract expectations
		List<Corpus.Edge> edges = c.getEdges();
		for(int e = 0; e < edges.size(); ++e)
		{
			double [] q = pco.posterior(e);
			Corpus.Edge edge = edges.get(e);

			TIntArrayList context = edge.getContext();
			double contextCnt = edge.getCount();
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
		double[] prob;
		
		if(edge.getTag()>=0){
			prob=new double[K];
			prob[edge.getTag()]=1;
			return prob;
		}
		
		if (edge.getPhraseId() < n_phrases)
			prob = Arrays.copyOf(pi[edge.getPhraseId()], K);
		else
		{
			prob = new double[K];
			Arrays.fill(prob, 1.0);
		}
		
		TIntArrayList ctx = edge.getContext();
		for(int tag=0;tag<K;tag++)
		{
			for(int c=0;c<n_positions;c++)
			{
				int word = ctx.get(c);
				if (!this.c.isSentinel(word) && word < n_words)
					prob[tag]*=emit[tag][c][word];
			}
		}
		
		return prob;
	}
	
	public void displayPosterior(PrintStream ps, List<Edge> testing)
	{	
		for (Edge edge : testing)
		{
			double probs[] = posterior(edge);
			arr.F.l1normalize(probs);

			// emit phrase
			ps.print(edge.getPhraseString());
			ps.print("\t");
			ps.print(edge.getContextString(true));
			int t=arr.F.argmax(probs);
			ps.println(" ||| C=" + t + " T=" + edge.getCount() + " P=" + probs[t]);
			//ps.println("# probs " + Arrays.toString(probs));
		}
	}
	
	public void displayModelParam(PrintStream ps)
	{
		final double EPS = 1e-6;
		ps.println("phrases " + n_phrases + " tags " + K + " positions " + n_positions);
		
		for (int i = 0; i < n_phrases; ++i)
			for(int j=0;j<pi[i].length;j++)
				if (pi[i][j] > EPS)
					ps.println(i + " " + j + " " + pi[i][j]);

		ps.println();
		for (int i = 0; i < K; ++i)
		{
			for(int position=0;position<n_positions;position++)
			{
				for(int word=0;word<emit[i][position].length;word++)
				{
					if (emit[i][position][word] > EPS)
						ps.println(i + " " + position + " " + word + " " + emit[i][position][word]);
				}
			}
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

	public void loadParameters(BufferedReader input) throws IOException
	{	
		final double EPS = 1e-50;
		
		// overwrite pi, emit with ~zeros
		for(double [][]i:emit)
			for(double []j:i)
				Arrays.fill(j, EPS);

		for(double []j:pi)
			Arrays.fill(j, EPS);

		String line = input.readLine();
		assert line != null;

		Pattern space = Pattern.compile(" +");
		String[] parts = space.split(line);
		assert parts.length == 6;

		assert parts[0].equals("phrases");
		int phrases = Integer.parseInt(parts[1]);
		int tags = Integer.parseInt(parts[3]);
		int positions = Integer.parseInt(parts[5]);
		
		assert phrases == n_phrases;
		assert tags == K;
		assert positions == n_positions;

		// read in pi
		while ((line = input.readLine()) != null)
		{
			line = line.trim();
			if (line.isEmpty()) break;
			
			String[] tokens = space.split(line);
			assert tokens.length == 3;
			int p = Integer.parseInt(tokens[0]);
			int t = Integer.parseInt(tokens[1]);
			double v = Double.parseDouble(tokens[2]);

			pi[p][t] = v;
		}
		
		// read in emissions
		while ((line = input.readLine()) != null)
		{
			String[] tokens = space.split(line);
			assert tokens.length == 4;
			int t = Integer.parseInt(tokens[0]);
			int p = Integer.parseInt(tokens[1]);
			int w = Integer.parseInt(tokens[2]);
			double v = Double.parseDouble(tokens[3]);

			emit[t][p][w] = v;
		}
	}
}
