package phrase;

import io.FileUtil;

import java.io.PrintStream;
import java.util.Arrays;

public class PhraseCluster {

	/**@brief number of clusters*/
	public int K;
	private int n_phrase;
	private int n_words;
	public PhraseCorpus c;
	
	/**@brief
	 * emit[tag][position][word]
	 */
	private double emit[][][];
	private double pi[][];
	
	public static int ITER=20;
	public static String postFilename="../pdata/posterior.out";
	public static String phraseStatFilename="../pdata/phrase_stat.out";
	private static int NUM_TAG=3;
	public static void main(String[] args) {
		
		PhraseCorpus c=new PhraseCorpus(PhraseCorpus.DATA_FILENAME);
		
		PhraseCluster cluster=new PhraseCluster(NUM_TAG,c);
		PhraseObjective.ps=FileUtil.openOutFile(phraseStatFilename);
		for(int i=0;i<ITER;i++){
			PhraseObjective.ps.println("ITER: "+i);
			cluster.PREM();
		//	cluster.EM();
		}
		
		PrintStream ps=io.FileUtil.openOutFile(postFilename);
		cluster.displayPosterior(ps);
		ps.println();
		cluster.displayModelParam(ps);
		ps.close();
		PhraseObjective.ps.close();
	}

	public PhraseCluster(int numCluster,PhraseCorpus corpus){
		K=numCluster;
		c=corpus;
		n_words=c.wordLex.size();
		n_phrase=c.data.length;
		
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
		
		pi[0]=new double[]{
			0.3,0.5,0.2
		};
		
		double temp[][]=new double[][]{
				{0.11,0.16,0.19,0.11,0.1},
				{0.10,0.15,0.18,0.1,0.11},
				{0.09,0.07,0.12,0.14,0.13} 
		};
		
		for(int tag=0;tag<3;tag++){
			for(int word=0;word<4;word++){
				for(int pos=0;pos<4;pos++){
					emit[tag][pos][word]=temp[tag][word];
				}          
			}
		}
		
	}
		
	public void EM(){
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
	}
	
	public void PREM(){
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
