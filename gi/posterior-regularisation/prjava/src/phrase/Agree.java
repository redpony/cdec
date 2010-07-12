package phrase;

import gnu.trove.TIntArrayList;

import io.FileUtil;

import java.io.File;
import java.io.IOException;
import java.io.PrintStream;
import java.util.List;

import phrase.Corpus.Edge;

public class Agree {
	private PhraseCluster model1;
	private C2F model2;
	Corpus c;
	private int K,n_phrases, n_words, n_contexts, n_positions1,n_positions2;
	
	/**@brief sum of loglikelihood of two
	 * individual models
	 */
	public double llh;
	/**@brief Bhattacharyya distance
	 * 
	 */
	public double bdist; 
	/**
	 * 
	 * @param numCluster
	 * @param corpus
	 */
	public Agree(int numCluster, Corpus corpus){
		
		model1=new PhraseCluster(numCluster, corpus, 0, 0, 0);
		model2=new C2F(numCluster,corpus);
		c=corpus;
		n_words=c.getNumWords();
		n_phrases=c.getNumPhrases();
		n_contexts=c.getNumContexts();
		n_positions1=c.getNumContextPositions();
		n_positions2=2;
		K=numCluster;
		
	}
	
	/**@brief test
	 * 
	 */
	public static void main(String args[]){
		//String in="../pdata/canned.con";
		String in="../pdata/btec.con";
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
		
		Agree agree=new Agree(numCluster, corpus);
		int iter=20;
		for(int i=0;i<iter;i++){
			agree.EM();
			System.out.println("Iter"+i+", llh: "+agree.llh+
					", divergence:"+agree.bdist+
							" sum: "+(agree.llh+agree.bdist));
		}
		
		File outfile = new File (out);
		try {
			PrintStream ps = FileUtil.printstream(outfile);
			agree.displayPosterior(ps);
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
		
		double [][][]exp_emit1=new double [K][n_positions1][n_words];
		double [][]exp_pi1=new double[n_phrases][K];
		
		double [][][]exp_emit2=new double [K][n_positions2][n_words];
		double [][]exp_pi2=new double[n_contexts][K];
		
		llh=0;
		bdist=0;
		//E
		for(int context=0; context< n_contexts; context++){
			
			List<Edge> contexts = c.getEdgesForContext(context);

			for (int ctx=0; ctx<contexts.size(); ctx++){
				Edge edge = contexts.get(ctx);
				int phrase=edge.getPhraseId();
				double p[]=posterior(edge);
				double z = arr.F.l1norm(p);
				assert z > 0;
				bdist += edge.getCount() * Math.log(z);
				arr.F.l1normalize(p);
				
				int count = edge.getCount();
				//increment expected count
				TIntArrayList phraseToks = edge.getPhrase();
				TIntArrayList contextToks = edge.getContext();
				for(int tag=0;tag<K;tag++){

					for(int position=0;position<n_positions1;position++){
						exp_emit1[tag][position][contextToks.get(position)]+=p[tag]*count;
					}
					
					exp_emit2[tag][0][phraseToks.get(0)]+=p[tag]*count;
					exp_emit2[tag][1][phraseToks.get(phraseToks.size()-1)]+=p[tag]*count;
					
					exp_pi1[phrase][tag]+=p[tag]*count;
					exp_pi2[context][tag]+=p[tag]*count;
				}
			}
		}
		
		//System.out.println("Log likelihood: "+loglikelihood);
		
		//M
		for(double [][]i:exp_emit1){
			for(double []j:i){
				arr.F.l1normalize(j);
			}
		}
		
		for(double []j:exp_pi1){
			arr.F.l1normalize(j);
		}
		
		for(double [][]i:exp_emit2){
			for(double []j:i){
				arr.F.l1normalize(j);
			}
		}
		
		for(double []j:exp_pi2){
			arr.F.l1normalize(j);
		}
		
		model1.emit=exp_emit1;
		model1.pi=exp_pi1;
		model2.emit=exp_emit2;
		model2.pi=exp_pi2;
		
		return llh;
	}

	public double[] posterior(Corpus.Edge edge) 
	{
		double[] prob1=model1.posterior(edge);
		double[] prob2=model2.posterior(edge);
		
		llh+=edge.getCount()*Math.log(arr.F.l1norm(prob1));
		llh+=edge.getCount()*Math.log(arr.F.l1norm(prob2));
		arr.F.l1normalize(prob1);
		arr.F.l1normalize(prob2);
		
		for(int i=0;i<prob1.length;i++){
			prob1[i]*=prob2[i];
			prob1[i]=Math.sqrt(prob1[i]);
		}
		
		return prob1;
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
	
}
