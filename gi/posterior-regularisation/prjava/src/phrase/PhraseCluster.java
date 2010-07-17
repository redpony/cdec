package phrase;

import gnu.trove.TIntArrayList;
import org.apache.commons.math.special.Gamma;

import java.io.BufferedReader;
import java.io.File;
import java.io.IOException;
import java.io.PrintStream;
import java.util.Arrays;
import java.util.List;
import java.util.StringTokenizer;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicLong;
import java.util.regex.Pattern;

import phrase.Corpus.Edge;


public class PhraseCluster {
	
	public int K;
	private int n_phrases, n_words, n_contexts, n_positions, edge_threshold;
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
		edge_threshold=0;

		emit=new double [K][n_positions][n_words];
		pi=new double[n_phrases][K];
		
		for(double [][]i:emit)
			for(double []j:i)
				arr.F.randomise(j, true);

		for(double []j:pi)
			arr.F.randomise(j, true);
	}
	
	public void initialiseVB(double alphaEmit, double alphaPi)
	{
		assert alphaEmit > 0;
		assert alphaPi > 0;
		
		for(double [][]i:emit)
			for(double []j:i)
				digammaNormalize(j, alphaEmit);
		
		for(double []j:pi)
			digammaNormalize(j, alphaPi);
	}
	
	void useThreadPool(int threads)
	{
		assert threads > 0;
		pool = Executors.newFixedThreadPool(threads);
	}

	public double EM(boolean skipBigPhrases)
	{
		double [][][]exp_emit=new double [K][n_positions][n_words];
		double [][]exp_pi=new double[n_phrases][K];
		
		for(double [][]i:exp_emit)
			for(double []j:i)
				Arrays.fill(j, 1e-10);
		for(double []j:pi)
			Arrays.fill(j, 1e-10);
		
		double loglikelihood=0;
		
		//E
		for(int phrase=0; phrase < n_phrases; phrase++)
		{
			if (skipBigPhrases && c.getPhrase(phrase).size() >= 2)
			{
				System.arraycopy(pi[phrase], 0, exp_pi[phrase], 0, K);
				continue;
			}			

			List<Edge> contexts = c.getEdgesForPhrase(phrase);

			for (int ctx=0; ctx<contexts.size(); ctx++)
			{
				Edge edge = contexts.get(ctx);
				if (edge.getCount() < edge_threshold || c.isRare(edge)) 
					continue;
				
				double p[]=posterior(edge);
				double z = arr.F.l1norm(p);
				assert z > 0;
				loglikelihood += edge.getCount() * Math.log(z);
				arr.F.l1normalize(p);
				
				int count = edge.getCount();
				//increment expected count
				TIntArrayList context = edge.getContext();
				for(int tag=0;tag<K;tag++)
				{
					for(int pos=0;pos<n_positions;pos++)
						exp_emit[tag][pos][context.get(pos)]+=p[tag]*count;		
					exp_pi[phrase][tag]+=p[tag]*count;
				}
			}
		}

		//M
		for(double [][]i:exp_emit)
			for(double []j:i)
				arr.F.l1normalize(j);
		
		for(double []j:exp_pi)
			arr.F.l1normalize(j);
			
		emit=exp_emit;
		pi=exp_pi;

		return loglikelihood;
	}
	
	public double VBEM(double alphaEmit, double alphaPi, boolean skipBigPhrases)
	{
		// FIXME: broken - needs to be done entirely in log-space
		assert !skipBigPhrases : "FIXME: implement this!";
		
		double [][][]exp_emit = new double [K][n_positions][n_words];
		double [][]exp_pi = new double[n_phrases][K];
		
		double loglikelihood=0;
		
		//E
		for(int phrase=0; phrase < n_phrases; phrase++)
		{
			List<Edge> contexts = c.getEdgesForPhrase(phrase);

			for (int ctx=0; ctx<contexts.size(); ctx++)
			{
				Edge edge = contexts.get(ctx);
				double p[] = posterior(edge);
				double z = arr.F.l1norm(p);
				assert z > 0;
				loglikelihood += edge.getCount() * Math.log(z);
				arr.F.l1normalize(p);
				
				int count = edge.getCount();
				//increment expected count
				TIntArrayList context = edge.getContext();
				for(int tag=0;tag<K;tag++)
				{
					for(int pos=0;pos<n_positions;pos++)
						exp_emit[tag][pos][context.get(pos)] += p[tag]*count;		
					exp_pi[phrase][tag] += p[tag]*count;
				}
			}
		}

		// find the KL terms, KL(q||p) where p is symmetric Dirichlet prior and q are the expectations 
		double kl = 0;
		for (int phrase=0; phrase < n_phrases; phrase++)
			kl += KL_symmetric_dirichlet(exp_pi[phrase], alphaPi);
	
		for (int tag=0;tag<K;tag++)
			for (int pos=0;pos<n_positions; ++pos)
				kl += this.KL_symmetric_dirichlet(exp_emit[tag][pos], alphaEmit); 
		// FIXME: exp_emit[tag][pos] has structural zeros - certain words are *never* seen in that position

		//M
		for(double [][]i:exp_emit)
			for(double []j:i)
				digammaNormalize(j, alphaEmit);
		emit=exp_emit;
		for(double []j:exp_pi)
			digammaNormalize(j, alphaPi);
		pi=exp_pi;

		System.out.println("KL=" + kl + " llh=" + loglikelihood);
		System.out.println(Arrays.toString(pi[0]));
		System.out.println(Arrays.toString(exp_emit[0][0]));
		return kl + loglikelihood;
	}
	
	public void digammaNormalize(double [] a, double alpha)
	{
		double sum=0;
		for(int i=0;i<a.length;i++)
			sum += a[i];
		
		assert sum > 1e-20;
		double dgs = Gamma.digamma(sum + alpha);
		
		for(int i=0;i<a.length;i++)
			a[i] = Math.exp(Gamma.digamma(a[i] + alpha/a.length) - dgs);
	}
	
	private double KL_symmetric_dirichlet(double[] q, double alpha)
	{
		// assumes that zeros in q are structural & should be skipped
		// FIXME: asssumption doesn't hold
		
		double p0 = alpha;
		double q0 = 0;
		int n = 0;
		for (int i=0; i<q.length; i++)
		{
			if (q[i] > 0)
			{
				q0 += q[i];
				n += 1;
			}
		}

		double kl = Gamma.logGamma(q0) - Gamma.logGamma(p0);
		kl += n * Gamma.logGamma(alpha / n);
		double digamma_q0 = Gamma.digamma(q0);
		for (int i=0; i<q.length; i++)
		{
			if (q[i] > 0)
				kl -= -Gamma.logGamma(q[i]) - (q[i] - alpha/q.length) * (Gamma.digamma(q[i]) - digamma_q0);
		}
		return kl;
	}
	
	public double PREM(double scalePT, double scaleCT, boolean skipBigPhrases)
	{
		if (scaleCT == 0)
		{
			if (pool != null)
				return PREM_phrase_constraints_parallel(scalePT, skipBigPhrases);
			else
				return PREM_phrase_constraints(scalePT, skipBigPhrases);
		}
		else
			return this.PREM_phrase_context_constraints(scalePT, scaleCT, skipBigPhrases);
	}

	
	public double PREM_phrase_constraints(double scalePT, boolean skipBigPhrases)
	{
		double [][][]exp_emit=new double[K][n_positions][n_words];
		double [][]exp_pi=new double[n_phrases][K];
		
		for(double [][]i:exp_emit)
			for(double []j:i)
				Arrays.fill(j, 1e-10);
		for(double []j:pi)
			Arrays.fill(j, 1e-10);

		if (lambdaPT == null && cacheLambda)
			lambdaPT = new double[n_phrases][];
		
		double loglikelihood=0, kl=0, l1lmax=0, primal=0;
		int failures=0, iterations=0;
		long start = System.currentTimeMillis();
		//E
		for(int phrase=0; phrase<n_phrases; phrase++){
			if (skipBigPhrases && c.getPhrase(phrase).size() >= 2)
			{
				System.arraycopy(pi[phrase], 0, exp_pi[phrase], 0, K);
				continue;
			}
			
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
		
		for(double []j:exp_pi)
			arr.F.l1normalize(j);
		pi=exp_pi;
		
		return primal;
	}

	public double PREM_phrase_constraints_parallel(final double scalePT, boolean skipBigPhrases)
	{
		assert(pool != null);
		
		final LinkedBlockingQueue<PhraseObjective> expectations 
			= new LinkedBlockingQueue<PhraseObjective>();
		
		double [][][]exp_emit=new double [K][n_positions][n_words];
		double [][]exp_pi=new double[n_phrases][K];
		
		if (skipBigPhrases)
		{
			for(double [][]i:exp_emit)
				for(double []j:i)
					Arrays.fill(j, 1e-100);
		}
		
		double loglikelihood=0, kl=0, l1lmax=0, primal=0;
		final AtomicInteger failures = new AtomicInteger(0);
		final AtomicLong elapsed = new AtomicLong(0l);
		int iterations=0, n=n_phrases;
		long start = System.currentTimeMillis();
		
		if (lambdaPT == null && cacheLambda)
			lambdaPT = new double[n_phrases][];

		//E
		for(int phrase=0;phrase<n_phrases;phrase++){
			if (skipBigPhrases && c.getPhrase(phrase).size() >= 2)
			{
				n -= 1;
				System.arraycopy(pi[phrase], 0, exp_pi[phrase], 0, K);
				continue;
			}
			final int p=phrase;
			pool.execute(new Runnable() {
				public void run() {
					try {
						//System.out.println("" + Thread.currentThread().getId() + " optimising lambda for " + p);
						long start = System.currentTimeMillis();
						PhraseObjective po = new PhraseObjective(PhraseCluster.this, p, scalePT, (cacheLambda) ? lambdaPT[p] : null);
						boolean ok = po.optimizeWithProjectedGradientDescent();
						if (!ok) failures.incrementAndGet();
						long end = System.currentTimeMillis();
						elapsed.addAndGet(end - start);

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
		for(int count=0;count<n;count++) {
			try {
				//System.out.println("" + Thread.currentThread().getId() + " reading queue #" + count);

				// wait (blocking) until something is ready
				PhraseObjective po = expectations.take();
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
	
	public double PREM_phrase_context_constraints(double scalePT, double scaleCT, boolean skipBigPhrases)
	{	
		assert !skipBigPhrases : "Not supported yet - FIXME!"; //FIXME
		
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
		
		//if (edge.getCount() < edge_threshold)
			//System.out.println("Edge: " + edge + " probs for phrase " + Arrays.toString(prob));
		
		TIntArrayList ctx = edge.getContext();
		for(int tag=0;tag<K;tag++)
		{
			for(int c=0;c<n_positions;c++)
			{
				int word = ctx.get(c);
				//if (edge.getCount() < edge_threshold)
					//System.out.println("\ttag: " + tag + " context word: " + word + " prob " + emit[tag][c][word]);

				if (!this.c.isSentinel(word))
					prob[tag]*=emit[tag][c][word];
			}
		}

		//if (edge.getCount() < edge_threshold)
			//System.out.println("prob " + Arrays.toString(prob));
		
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

	public void setEdgeThreshold(int edgeThreshold) 
	{
		this.edge_threshold = edgeThreshold;
	}
}
