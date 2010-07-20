package phrase;

import gnu.trove.TIntArrayList;

import io.FileUtil;

import java.io.File;
import java.io.IOException;
import java.io.PrintStream;
import java.util.Arrays;
import java.util.List;

import phrase.Corpus.Edge;

/**
 * @brief context generates phrase
 * @author desaic
 *
 */
public class C2F {
	public int K;
	private int n_words, n_contexts, n_positions;
	public Corpus c;
	
	/**@brief
	 *  emit[tag][position][word] = p(word | tag, position in phrase)
	 */
	public double emit[][][];
	/**@brief
	 *  pi[context][tag] = p(tag | context)
	 */
	public double pi[][];
	
	public C2F(int numCluster, Corpus corpus){
		K=numCluster;
		c=corpus;
		n_words=c.getNumWords();
		n_contexts=c.getNumContexts();
		
		//number of words in a phrase to be considered
		//currently the first and last word in source and target
		//if the phrase has length 1 in either dimension then
		//we use the same word for two positions
		n_positions=c.phraseEdges(c.getEdges().get(0).getPhrase()).size();
		
		emit=new double [K][n_positions][n_words];
		pi=new double[n_contexts][K];
		
		for(double [][]i:emit){
			for(double []j:i){
				arr.F.randomise(j);
			}
		}
		
		for(double []j:pi){
			arr.F.randomise(j);
		}
	}
	
	/**@brief test
	 * 
	 */
	public static void main(String args[]){
		String in="../pdata/canned.con";
		String out="../pdata/posterior.out";
		int numCluster=25;
		Corpus corpus = null;
		File infile = new File(in);
		try {
			System.out.println("Reading concordance from " + infile);
			corpus = Corpus.readFromFile(FileUtil.reader(infile));
			corpus.printStats(System.out);
		} catch (IOException e) {
			System.err.println("Failed to open input file: " + infile);
			e.printStackTrace();
			System.exit(1);
		}
		
		C2F c2f=new C2F(numCluster,corpus);
		int iter=20;
		double llh=0;
		for(int i=0;i<iter;i++){
			llh=c2f.EM();
			System.out.println("Iter"+i+", llh: "+llh);
		}
		
		File outfile = new File (out);
		try {
			PrintStream ps = FileUtil.printstream(outfile);
			c2f.displayPosterior(ps);
		//	ps.println();
		//	c2f.displayModelParam(ps);
			ps.close();
		} catch (IOException e) {
			System.err.println("Failed to open output file: " + outfile);
			e.printStackTrace();
			System.exit(1);
		}
		
	}
	
	public double EM(){
		double [][][]exp_emit=new double [K][n_positions][n_words];
		double [][]exp_pi=new double[n_contexts][K];
		
		double loglikelihood=0;
		
		//E
		for(int context=0; context< n_contexts; context++){
			
			List<Edge> contexts = c.getEdgesForContext(context);

			for (int ctx=0; ctx<contexts.size(); ctx++){
				Edge edge = contexts.get(ctx);
				double p[]=posterior(edge);
				double z = arr.F.l1norm(p);
				assert z > 0;
				loglikelihood += edge.getCount() * Math.log(z);
				arr.F.l1normalize(p);
				
				double count = edge.getCount();
				//increment expected count
				TIntArrayList phrase= edge.getPhrase();
				for(int tag=0;tag<K;tag++){

					exp_emit[tag][0][phrase.get(0)]+=p[tag]*count;
					exp_emit[tag][1][phrase.get(phrase.size()-1)]+=p[tag]*count;
					
					exp_pi[context][tag]+=p[tag]*count;
				}
			}
		}
		
		//System.out.println("Log likelihood: "+loglikelihood);
		
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

	public double[] posterior(Corpus.Edge edge) 
	{
		double[] prob=Arrays.copyOf(pi[edge.getContextId()], K);
		
		TIntArrayList phrase = edge.getPhrase();
		TIntArrayList offsets = c.phraseEdges(phrase);
		for(int tag=0;tag<K;tag++)
		{
			for (int i=0; i < offsets.size(); ++i)
				prob[tag]*=emit[tag][i][phrase.get(offsets.get(i))];
		}
			
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
		
		ps.println("P(tag|context)");
		for (int i = 0; i < n_contexts; ++i)
		{
			ps.print(c.getContext(i));
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
	
}
