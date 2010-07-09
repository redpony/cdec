package test;

import hmm.HMM;
import hmm.POS;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.PrintStream;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;

import data.Corpus;

public class HMMModelStats {

	public static String modelFilename="../posdata/posModel.out";
	public static String alphaFilename="../posdata/corpus.alphabet";
	public static String statsFilename="../posdata/model.stats";

	public static final int NUM_WORD=50;
	
	public static String testFilename="../posdata/en_test.conll";
	
	public static double [][]maxwt;
	
	public static void main(String[] args) {
		HashMap<String, Integer>vocab=
			(HashMap<String, Integer>) io.SerializedObjects.readSerializedObject(alphaFilename);
		
		Corpus test=new Corpus(testFilename,vocab);
		
		String [] dict=new String [vocab.size()+1];
		for(String key:vocab.keySet()){
			dict[vocab.get(key)]=key;
		}
		dict[dict.length-1]=Corpus.UNK_TOK;
		
		HMM hmm=new HMM();
		hmm.readModel(modelFilename);

		
		
		PrintStream ps = null;
		try {
			ps = io.FileUtil.printstream(new File(statsFilename));
		} catch (IOException e) {
			e.printStackTrace();
			System.exit(1);
		}
		
		double [][] emit=hmm.getEmitProb();
		for(int i=0;i<emit.length;i++){
			ArrayList<IntDoublePair>l=new ArrayList<IntDoublePair>();
			for(int j=0;j<emit[i].length;j++){
				l.add(new IntDoublePair(j,emit[i][j]));
			}
			Collections.sort(l);
			ps.println(i);
			for(int j=0;j<NUM_WORD;j++){
				if(j>=dict.length){
					break;
				}
				ps.print(dict[l.get(j).idx]+"\t");
				if((1+j)%10==0){
					ps.println();
				}
			}
			ps.println("\n");
		}
		
		checkMaxwt(hmm,ps,test.getAllData());
		
		int terminalSym=vocab.get(Corpus .END_SYM);
		//sample 10 sentences
		for(int i=0;i<10;i++){
			int []sent=hmm.sample(terminalSym);
			for(int j=0;j<sent.length;j++){
				ps.print(dict[sent[j]]+"\t");
			}
			ps.println();
		}
		
		ps.close();
		
	}
	
	public static void checkMaxwt(HMM hmm,PrintStream ps,int [][]data){
		double [][]emit=hmm.getEmitProb();
		maxwt=new double[emit.length][emit[0].length];
		
		hmm.computeMaxwt(maxwt,data);
		double sum=0;
		for(int i=0;i<maxwt.length;i++){
			for(int j=0;j<maxwt.length;j++){
				sum+=maxwt[i][j];
			}
		}
		
		ps.println("max w t P(w_i|t): "+sum);
		
	}
	
}
