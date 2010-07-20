package phrase;

import gnu.trove.TIntArrayList;

import io.FileUtil;

import java.io.File;
import java.io.IOException;
import java.io.PrintStream;
import java.util.List;

import phrase.Corpus.Edge;

public class Agree2Sides {
	PhraseCluster model1,model2;
	Corpus c1,c2;
	private int K;
	
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
	public Agree2Sides(int numCluster, Corpus corpus1 , Corpus corpus2 ){
		
		model1=new PhraseCluster(numCluster, corpus1);
		model2=new PhraseCluster(numCluster,corpus2);
		c1=corpus1;
		c2=corpus2;
		K=numCluster;
		
	}
	
	/**@brief test
	 * 
	 */
	public static void main(String args[]){
		//String in="../pdata/canned.con";
	//	String in="../pdata/btec.con";
		String in1="../pdata/source.txt";
		String in2="../pdata/target.txt";
		String out="../pdata/posterior.out";
		int numCluster=25;
		Corpus corpus1 = null,corpus2=null;
		File infile1 = new File(in1),infile2=new File(in2);
		try {
			System.out.println("Reading concordance from " + infile1);
			corpus1 = Corpus.readFromFile(FileUtil.reader(infile1));
			System.out.println("Reading concordance from " + infile2);
			corpus2 = Corpus.readFromFile(FileUtil.reader(infile2));
			corpus1.printStats(System.out);
		} catch (IOException e) {
			System.err.println("Failed to open input file: " + infile1);
			e.printStackTrace();
			System.exit(1);
		}
		
		Agree2Sides agree=new Agree2Sides(numCluster, corpus1,corpus2);
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
		
		double [][][]exp_emit1=new double [K][c1.getNumContextPositions()][c1.getNumWords()];
		double [][]exp_pi1=new double[c1.getNumPhrases()][K];
		
		double [][][]exp_emit2=new double [K][c2.getNumContextPositions()][c2.getNumWords()];
		double [][]exp_pi2=new double[c2.getNumPhrases()][K];
		
		llh=0;
		bdist=0;
		//E
		for(int i=0;i<c1.getEdges().size();i++){
			Edge edge1=c1.getEdges().get(i);
			Edge edge2=c2.getEdges().get(i);
			double p[]=posterior(i);
			double z = arr.F.l1norm(p);
			assert z > 0;
			bdist += edge1.getCount() * Math.log(z);
			arr.F.l1normalize(p);
			double count = edge1.getCount();
				//increment expected count
			TIntArrayList contextToks1 = edge1.getContext();
			TIntArrayList contextToks2 = edge2.getContext();
			int phrase1=edge1.getPhraseId();
			int phrase2=edge2.getPhraseId();
			for(int tag=0;tag<K;tag++){
				for(int position=0;position<c1.getNumContextPositions();position++){
					exp_emit1[tag][position][contextToks1.get(position)]+=p[tag]*count;
				}
				for(int position=0;position<c2.getNumContextPositions();position++){
					exp_emit2[tag][position][contextToks2.get(position)]+=p[tag]*count;
				}
				exp_pi1[phrase1][tag]+=p[tag]*count;
				exp_pi2[phrase2][tag]+=p[tag]*count;
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

	public double[] posterior(int edgeIdx) 
	{
		Edge edge1=c1.getEdges().get(edgeIdx);
		Edge edge2=c2.getEdges().get(edgeIdx);
		double[] prob1=model1.posterior(edge1);
		double[] prob2=model2.posterior(edge2);
		
		llh+=edge1.getCount()*Math.log(arr.F.l1norm(prob1));
		llh+=edge2.getCount()*Math.log(arr.F.l1norm(prob2));
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
		
		for (int i=0;i<c1.getEdges().size();i++)
		{
			Edge edge=c1.getEdges().get(i);
			double probs[] = posterior(i);
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
