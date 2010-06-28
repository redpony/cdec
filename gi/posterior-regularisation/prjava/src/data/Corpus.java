package data;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Scanner;

public class Corpus {

	public static final String alphaFilename="../posdata/corpus.alphabet";
	public static final String tagalphaFilename="../posdata/corpus.tag.alphabet";
	
//	public static final String START_SYM="<s>";
	public static final String END_SYM="<e>";
	public static final String NUM_TOK="<NUM>";
	
	public static final String UNK_TOK="<unk>";
	
	private ArrayList<String[]>sent;
	private ArrayList<int[]>data;
	
	public ArrayList<String[]>tag;
	public  ArrayList<int[]>tagData;
	
	public static boolean convertNumTok=true;
	
	private HashMap<String,Integer>freq;
	public HashMap<String,Integer>vocab;
	
	public HashMap<String,Integer>tagVocab;
	private int tagV;
	
	private int V;
	
	public static void main(String[] args) {
		Corpus c=new Corpus("../posdata/en_test.conll");
		System.out.println(
			Arrays.toString(c.get(0))	
		);
		System.out.println(
				Arrays.toString(c.getInt(0))	
			);
		
		System.out.println(
				Arrays.toString(c.get(1))	
			);
			System.out.println(
					Arrays.toString(c.getInt(1))	
				);
	}

	public Corpus(String filename,HashMap<String,Integer>dict){
		V=0;
		tagV=0;
		freq=new HashMap<String,Integer>();
		tagVocab=new HashMap<String,Integer>();
		vocab=dict;
		
		sent=new ArrayList<String[]>();
		tag=new ArrayList<String[]>();
		
		Scanner sc=io.FileUtil.openInFile(filename);
		ArrayList<String>s=new ArrayList<String>();
	//	s.add(START_SYM);
		while(sc.hasNextLine()){
			String line=sc.nextLine();
			String toks[]=line.split("\t");
			if(toks.length<2){
				s.add(END_SYM);
				sent.add(s.toArray(new String[0]));
				s=new ArrayList<String>();
		//		s.add(START_SYM);
				continue;
			}
			String tok=toks[1].toLowerCase();
			s.add(tok);
		}
		sc.close();

		buildData();
	}
	
	public Corpus(String filename){
		V=0;
		freq=new HashMap<String,Integer>();
		vocab=new HashMap<String,Integer>();
		tagVocab=new HashMap<String,Integer>();
		
		sent=new ArrayList<String[]>();
		tag=new ArrayList<String[]>();
		
		System.out.println("Reading:"+filename);
		
		Scanner sc=io.FileUtil.openInFile(filename);
		ArrayList<String>s=new ArrayList<String>();
		ArrayList<String>tags=new ArrayList<String>();
		//s.add(START_SYM);
		while(sc.hasNextLine()){
			String line=sc.nextLine();
			String toks[]=line.split("\t");
			if(toks.length<2){
				s.add(END_SYM);
				tags.add(END_SYM);
				if(s.size()>2){
					sent.add(s.toArray(new String[0]));
					tag.add(tags.toArray(new String [0]));
				}
				s=new ArrayList<String>();
				tags=new ArrayList<String>();
			//	s.add(START_SYM);
				continue;
			}
			
			String tok=toks[1].toLowerCase();
			if(convertNumTok && tok.matches(".*\\d.*")){
				tok=NUM_TOK;
			}
			s.add(tok);
			
			if(toks.length>3){
				tok=toks[3].toLowerCase();
			}else{
				tok="_";
			}
			tags.add(tok);
			
		}
		sc.close();
		
		for(int i=0;i<sent.size();i++){
			String[]toks=sent.get(i);
			for(int j=0;j<toks.length;j++){
				addVocab(toks[j]);
				addTag(tag.get(i)[j]);
			}
		}
		
		buildVocab();
		buildData();
		System.out.println(data.size()+"sentences, "+vocab.keySet().size()+" word types");
	}

	public String[] get(int idx){
		return sent.get(idx);
	}
	
	private void addVocab(String s){
		Integer integer=freq.get(s);
		if(integer==null){
			integer=0;
		}
		freq.put(s, integer+1);
	}
	
	public int tokIdx(String tok){
		Integer integer=vocab.get(tok);
		if(integer==null){
			return V;
		}
		return integer;
	}
	
	public int tagIdx(String tok){
		Integer integer=tagVocab.get(tok);
		if(integer==null){
			return tagV;
		}
		return integer;
	}
	
	private void buildData(){
		data=new ArrayList<int[]>();
		for(int i=0;i<sent.size();i++){
			String s[]=sent.get(i);
			data.add(new int [s.length]);
			for(int j=0;j<s.length;j++){
				data.get(i)[j]=tokIdx(s[j]);
			}
		}
		
		tagData=new ArrayList<int[]>();
		for(int i=0;i<tag.size();i++){
			String s[]=tag.get(i);
			tagData.add(new int [s.length]);
			for(int j=0;j<s.length;j++){
				tagData.get(i)[j]=tagIdx(s[j]);
			}
		}
	}
	
	public int [] getInt(int idx){
		return data.get(idx);
	}
	
	/**
	 * 
	 * @return size of vocabulary 
	 */
	public int getVocabSize(){
		return V;
	}
	
	public int [][]getAllData(){
		return data.toArray(new int [0][]);
	}
	
	public int [][]getTagData(){
		return tagData.toArray(new int [0][]);
	}
	
	private void buildVocab(){
		for (String key:freq.keySet()){
			if(freq.get(key)>2){
				vocab.put(key, V);
				V++;
			}
		}
		io.SerializedObjects.writeSerializedObject(vocab, alphaFilename);
		io.SerializedObjects.writeSerializedObject(tagVocab,tagalphaFilename);
	}

	private void addTag(String tag){
		Integer i=tagVocab.get(tag);
		if(i==null){
			tagVocab.put(tag, tagV);
			tagV++;
		}
	}
	
}
