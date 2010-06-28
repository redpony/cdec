package hmm;

import java.io.PrintStream;
import java.util.ArrayList;
import java.util.Scanner;

public class HMM {

	
	//trans[i][j]=prob of going FROM i to j
	double [][]trans;
	double [][]emit;
	double []pi;
	int  [][]data;
	int [][]tagdata;
	
	double logtrans[][];
	
	public HMMObjective o;
	
	public static void main(String[] args) {
	
	}
	
	public HMM(int n_state,int n_emit,int [][]data){
		trans=new double [n_state][n_state];
		emit=new double[n_state][n_emit];
		pi=new double [n_state];
		System.out.println(" random initial parameters");
		fillRand(trans);
		fillRand(emit);
		fillRand(pi);

		this.data=data;
		
	}
	
	private void fillRand(double [][] a){
		for(int i=0;i<a.length;i++){
			for(int j=0;j<a[i].length;j++){
				a[i][j]=Math.random();
			}
			l1normalize(a[i]);
		}
	}
	private void fillRand(double []a){
		for(int i=0;i<a.length;i++){
				a[i]=Math.random();
		}
		l1normalize(a);
	}
	
	private double loglikely=0;
	
	public void EM(){
		double trans_exp_cnt[][]=new double [trans.length][trans.length];
		double emit_exp_cnt[][]=new double[trans.length][emit[0].length];
		double start_exp_cnt[]=new double[trans.length];
		loglikely=0;
		
		//E
		for(int i=0;i<data.length;i++){
			
			double [][][] post=forwardBackward(data[i]);
			incrementExpCnt(post, data[i], 
					trans_exp_cnt,
					emit_exp_cnt,
					start_exp_cnt);
			
			
			if(i%100==0){
				System.out.print(".");
			}
			if(i%1000==0){
				System.out.println(i);
			}
			
		}
		System.out.println("Log likelihood: "+loglikely);
		
		//M
		addOneSmooth(emit_exp_cnt);
		for(int i=0;i<trans.length;i++){
		
			//transition probs
			double sum=0;
			for(int j=0;j<trans.length;j++){
				sum+=trans_exp_cnt[i][j];
			}
			//avoid NAN
			if(sum==0){
				sum=1;
			}
			for(int j=0;j<trans[i].length;j++){
				trans[i][j]=trans_exp_cnt[i][j]/sum;
			}
			
			//emission probs
			
			sum=0;
			for(int j=0;j<emit[i].length;j++){
				sum+=emit_exp_cnt[i][j];
			}
			//avoid NAN
			if(sum==0){
				sum=1;
			}
			for(int j=0;j<emit[i].length;j++){
				emit[i][j]=emit_exp_cnt[i][j]/sum;
			}
			
			
			//initial probs
			for(int j=0;j<pi.length;j++){
				pi[j]=start_exp_cnt[j];
			}
			l1normalize(pi);
		}
	}
	
	private double [][][]forwardBackward(int [] seq){
		double a[][]=new double [seq.length][trans.length];
		double b[][]=new double [seq.length][trans.length];
		
		int len=seq.length;
		//initialize the first step
		for(int i=0;i<trans.length;i++){
			a[0][i]=emit[i][seq[0]]*pi[i];
			b[len-1][i]=1;
		}
		
		//log of denominator for likelyhood
		double c=Math.log(l1norm(a[0]));
		
		l1normalize(a[0]);
		l1normalize(b[len-1]);
		
		
		
		//forward
		for(int n=1;n<len;n++){
			for(int i=0;i<trans.length;i++){
				for(int j=0;j<trans.length;j++){
					a[n][i]+=trans[j][i]*a[n-1][j];
				}
				a[n][i]*=emit[i][seq[n]];
			}
			c+=Math.log(l1norm(a[n]));
			l1normalize(a[n]);
		}
		
		loglikely+=c;
		
		//backward
		for(int n=len-2;n>=0;n--){
			for(int i=0;i<trans.length;i++){
				for(int j=0;j<trans.length;j++){
					b[n][i]+=trans[i][j]*b[n+1][j]*emit[j][seq[n+1]];
				}
			}
			l1normalize(b[n]);
		}
		
		
		//expected transition 
		double p[][][]=new double [seq.length][trans.length][trans.length];
		for(int n=0;n<len-1;n++){
			for(int i=0;i<trans.length;i++){
				for(int j=0;j<trans.length;j++){
					p[n][i][j]=a[n][i]*trans[i][j]*emit[j][seq[n+1]]*b[n+1][j];
					
				}
			}

			l1normalize(p[n]);
		}
		return p;
	}
	
	private void incrementExpCnt(
			double post[][][],int [] seq, 
			double trans_exp_cnt[][],
			double emit_exp_cnt[][],
			double start_exp_cnt[])
	{
		
		for(int n=0;n<post.length;n++){
			for(int i=0;i<trans.length;i++){
				double py=0;
				for(int j=0;j<trans.length;j++){
					py+=post[n][i][j];
					trans_exp_cnt[i][j]+=post[n][i][j];
				}

				emit_exp_cnt[i][seq[n]]+=py;				
				
			}
		}
		
		//the first state
		for(int i=0;i<trans.length;i++){
			double py=0;
			for(int j=0;j<trans.length;j++){
				py+=post[0][i][j];
			}
			start_exp_cnt[i]+=py;	
		}
		
		
		//the last state
		int len=post.length;
		for(int i=0;i<trans.length;i++){
			double py=0;
			for(int j=0;j<trans.length;j++){
				py+=post[len-2][j][i];
			}
			emit_exp_cnt[i][seq[len-1]]+=py;	
		}
	}
	
	public void l1normalize(double [] a){
		double sum=0;
		for(int i=0;i<a.length;i++){
			sum+=a[i];
		}
		if(sum==0){
			return ;
		}
		for(int i=0;i<a.length;i++){
			a[i]/=sum;
		}
	}
	
	public  void l1normalize(double [][] a){
		double sum=0;
		for(int i=0;i<a.length;i++){
			for(int j=0;j<a[i].length;j++){
				sum+=a[i][j];
			}
		}
		if(sum==0){
			return;
		}
		for(int i=0;i<a.length;i++){
			for(int j=0;j<a[i].length;j++){
				a[i][j]/=sum;
			}
		}
	}
	
	public void writeModel(String modelFilename){
		PrintStream ps=io.FileUtil.openOutFile(modelFilename);
		ps.println(trans.length);
		ps.println("Initial Probabilities:");
		for(int i=0;i<pi.length;i++){
			ps.print(pi[i]+"\t");
		}
		ps.println();
		ps.println("Transition Probabilities:");
		for(int i=0;i<trans.length;i++){
			for(int j=0;j<trans[i].length;j++){
				ps.print(trans[i][j]+"\t");
			}
			ps.println();
		}
		ps.println("Emission Probabilities:");
		ps.println(emit[0].length);
		for(int i=0;i<trans.length;i++){
			for(int j=0;j<emit[i].length;j++){
				ps.println(emit[i][j]);
			}
			ps.println();
		}
		ps.close();
	}
	
	public HMM(){
	
	}
	
	public void readModel(String modelFilename){
		Scanner sc=io.FileUtil.openInFile(modelFilename);
	
		int n_state=sc.nextInt();
		sc.nextLine();
		sc.nextLine();
		pi=new double [n_state];
		for(int i=0;i<n_state;i++){
			pi[i]=sc.nextDouble();
		}
		sc.nextLine();
		sc.nextLine();
		trans=new double[n_state][n_state];
		for(int i=0;i<trans.length;i++){
			for(int j=0;j<trans[i].length;j++){
				trans[i][j]=sc.nextDouble();
			}
		}
		sc.nextLine();
		sc.nextLine();
		
		int n_obs=sc.nextInt();
		emit=new double[n_state][n_obs];
		for(int i=0;i<trans.length;i++){
			for(int j=0;j<emit[i].length;j++){
				emit[i][j]=sc.nextDouble();
			}
		}
		sc.close();
	}
	
	public int []viterbi(int [] seq){
		double [][]p=new double [seq.length][trans.length];
		int backp[][]=new int [seq.length][trans.length];
		
		for(int i=0;i<trans.length;i++){
			p[0][i]=Math.log(emit[i][seq[0]]*pi[i]);
		}
		
		double a[][]=logtrans;
		if(logtrans==null){
			a=new double [trans.length][trans.length];
			for(int i=0;i<trans.length;i++){
				for(int j=0;j<trans.length;j++){
					a[i][j]=Math.log(trans[i][j]);
				}
			}
			logtrans=a;
		}
		
		double maxprob=0;
		for(int n=1;n<seq.length;n++){
			for(int i=0;i<trans.length;i++){
				maxprob=p[n-1][0]+a[0][i];
				backp[n][i]=0;
				for(int j=1;j<trans.length;j++){
					double prob=p[n-1][j]+a[j][i];
					if(maxprob<prob){
						backp[n][i]=j;
						maxprob=prob;
					}
				}
				p[n][i]=maxprob+Math.log(emit[i][seq[n]]);
			}
		}
		
		maxprob=p[seq.length-1][0];
		int maxIdx=0;
		for(int i=1;i<trans.length;i++){
			if(p[seq.length-1][i]>maxprob){
				maxprob=p[seq.length-1][i];
				maxIdx=i;
			}
		}
		int ans[]=new int [seq.length];
		ans[seq.length-1]=maxIdx;
		for(int i=seq.length-2;i>=0;i--){
			ans[i]=backp[i+1][ans[i+1]];
		}
		return ans;
	}
	
	public double l1norm(double a[]){
		double norm=0;
		for(int i=0;i<a.length;i++){
			norm += a[i];
		}
		return norm;
	}
	
	public double [][]getEmitProb(){
		return emit;
	}
	
	public int [] sample(int terminalSym){
		ArrayList<Integer > s=new ArrayList<Integer>();
		int state=sample(pi);
		int sym=sample(emit[state]);
		while(sym!=terminalSym){
			s.add(sym);
			state=sample(trans[state]);
			sym=sample(emit[state]);
		}
		
		int ans[]=new int [s.size()];
		for(int i=0;i<ans.length;i++){
			ans[i]=s.get(i);
		}
		return ans;
	}
	
	public int sample(double p[]){
		double r=Math.random();
		double sum=0;
		for(int i=0;i<p.length;i++){
			sum+=p[i];
			if(sum>=r){
				return i;
			}
		}
		return p.length-1;
	}
	
	public void train(int tagdata[][]){
		double trans_exp_cnt[][]=new double [trans.length][trans.length];
		double emit_exp_cnt[][]=new double[trans.length][emit[0].length];
		double start_exp_cnt[]=new double[trans.length];
		
		for(int i=0;i<tagdata.length;i++){
			start_exp_cnt[tagdata[i][0]]++;
			
			for(int j=0;j<tagdata[i].length;j++){
				if(j+1<tagdata[i].length){
					trans_exp_cnt[ tagdata[i][j] ] [ tagdata[i][j+1] ]++;
				}
				emit_exp_cnt[tagdata[i][j]][data[i][j]]++;
			}
			
		}
		
		//M
		addOneSmooth(emit_exp_cnt);
		for(int i=0;i<trans.length;i++){
		
			//transition probs
			double sum=0;
			for(int j=0;j<trans.length;j++){
				sum+=trans_exp_cnt[i][j];
			}
			if(sum==0){
				sum=1;
			}
			for(int j=0;j<trans[i].length;j++){
				trans[i][j]=trans_exp_cnt[i][j]/sum;
			}
			
			//emission probs

			sum=0;
			for(int j=0;j<emit[i].length;j++){
				sum+=emit_exp_cnt[i][j];
			}
			if(sum==0){
				sum=1;
			}
			for(int j=0;j<emit[i].length;j++){
				emit[i][j]=emit_exp_cnt[i][j]/sum;
			}

			
			//initial probs
			for(int j=0;j<pi.length;j++){
				pi[j]=start_exp_cnt[j];
			}
			l1normalize(pi);
		}
	}
	
	private void addOneSmooth(double a[][]){
		for(int i=0;i<a.length;i++){
			for(int j=0;j<a[i].length;j++){
				a[i][j]+=0.01;
			}
			//l1normalize(a[i]);
		}
	}
	
	public void PREM(){
		
		o.optimizeWithProjectedGradientDescent();
		
		double trans_exp_cnt[][]=new double [trans.length][trans.length];
		double emit_exp_cnt[][]=new double[trans.length][emit[0].length];
		double start_exp_cnt[]=new double[trans.length];
		
		o.loglikelihood=0;
		//E
		for(int sentNum=0;sentNum<data.length;sentNum++){
			
			double [][][] post=o.forwardBackward(sentNum);
			incrementExpCnt(post, data[sentNum], 
					trans_exp_cnt,
					emit_exp_cnt,
					start_exp_cnt);
			
			
			if(sentNum%100==0){
				System.out.print(".");
			}
			if(sentNum%1000==0){
				System.out.println(sentNum);
			}
			
		}
		
		System.out.println("Log likelihood: "+o.getValue());
		
		//M
		addOneSmooth(emit_exp_cnt);
		for(int i=0;i<trans.length;i++){
		
			//transition probs
			double sum=0;
			for(int j=0;j<trans.length;j++){
				sum+=trans_exp_cnt[i][j];
			}
			//avoid NAN
			if(sum==0){
				sum=1;
			}
			for(int j=0;j<trans[i].length;j++){
				trans[i][j]=trans_exp_cnt[i][j]/sum;
			}
			
			//emission probs
			
			sum=0;
			for(int j=0;j<emit[i].length;j++){
				sum+=emit_exp_cnt[i][j];
			}
			//avoid NAN
			if(sum==0){
				sum=1;
			}
			for(int j=0;j<emit[i].length;j++){
				emit[i][j]=emit_exp_cnt[i][j]/sum;
			}
			
			
			//initial probs
			for(int j=0;j<pi.length;j++){
				pi[j]=start_exp_cnt[j];
			}
			l1normalize(pi);
		}
		
	}
	
	public void computeMaxwt(double[][]maxwt, int[][] d){

		for(int sentNum=0;sentNum<d.length;sentNum++){
			double post[][][]=forwardBackward(d[sentNum]);
			
			for(int n=0;n<post.length;n++){
				for(int i=0;i<trans.length;i++){
					double py=0;
					for(int j=0;j<trans.length;j++){
						py+=post[n][i][j];
					}

					if(py>maxwt[i][d[sentNum][n]]){
						maxwt[i][d[sentNum][n]]=py;
					}
					
				}
			}
			
			//the last state
			int len=post.length;
			for(int i=0;i<trans.length;i++){
				double py=0;
				for(int j=0;j<trans.length;j++){
					py+=post[len-2][j][i];
				}
				
				if(py>maxwt[i][d[sentNum][len-1]]){
					maxwt[i][d[sentNum][len-1]]=py;
				}
				
			}
			
		}
	
	}
	
}//end of class
