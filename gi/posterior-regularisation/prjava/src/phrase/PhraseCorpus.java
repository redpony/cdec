package phrase;

import java.io.BufferedInputStream;
import java.io.BufferedReader;
import java.io.IOException;
import java.io.PrintStream;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Scanner;

public class PhraseCorpus {
	
	public static String LEX_FILENAME="../pdata/lex.out";
	public static String DATA_FILENAME="../pdata/btec.con";
	
	public HashMap<String,Integer>wordLex;
	public HashMap<String,Integer>phraseLex;
	
	public String wordList[];
	public String phraseList[];
	
	//data[phrase][num context][position]
	public int data[][][];
	public int numContexts;
	
	public static void main(String[] args) {
		// TODO Auto-generated method stub
		PhraseCorpus c=new PhraseCorpus(DATA_FILENAME);
		c.saveLex(LEX_FILENAME);
		c.loadLex(LEX_FILENAME);
		c.saveLex(LEX_FILENAME);
	}
	
	public PhraseCorpus(String filename){
		BufferedReader r=io.FileUtil.openBufferedReader(filename);
		
		phraseLex=new HashMap<String,Integer>();
		wordLex=new HashMap<String,Integer>();
		
		ArrayList<int[][]>dataList=new ArrayList<int[][]>();
		String line=null;
		numContexts = 0;
		
		while((line=readLine(r))!=null){
			
			String toks[]=line.split("\t");
			String phrase=toks[0];
			addLex(phrase,phraseLex);
			
			toks=toks[1].split(" \\|\\|\\| ");
			
			ArrayList <int[]>ctxList=new ArrayList<int[]>();
			
			for(int i=0;i<toks.length;i+=2){
				String ctx=toks[i];
				String words[]=ctx.split(" ");
				if (numContexts == 0)
					numContexts = words.length - 1;
				else
					assert numContexts == words.length - 1;
				
				int []context=new int [numContexts+1];
				int idx=0;
				for(String word:words){
					if(word.equals("<PHRASE>")){
						continue;
					}
					addLex(word,wordLex);
					context[idx]=wordLex.get(word);
					idx++;
				}
				
				String count=toks[i+1];
				context[idx]=Integer.parseInt(count.trim().substring(2));
				
				ctxList.add(context);
			}
			
			dataList.add(ctxList.toArray(new int [0][]));
			
		}
		try{
		r.close();
		}catch(IOException ioe){
			ioe.printStackTrace();
		}
		data=dataList.toArray(new int[0][][]);
	}

	private void addLex(String key, HashMap<String,Integer>lex){
		Integer i=lex.get(key);
		if(i==null){
			lex.put(key, lex.size());
		}
	}
	
	//for debugging
	public void saveLex(String lexFilename){
		PrintStream ps=io.FileUtil.openOutFile(lexFilename);
		ps.println("Phrase Lexicon");
		ps.println(phraseLex.size());
		printDict(phraseLex,ps);
		
		ps.println("Word Lexicon");
		ps.println(wordLex.size());
		printDict(wordLex,ps);
		ps.close();
	}
	
	private static void printDict(HashMap<String,Integer>lex,PrintStream ps){
		String []dict=buildList(lex);
		for(int i=0;i<dict.length;i++){
			ps.println(dict[i]);
		}
	}
	
	public void loadLex(String lexFilename){
		Scanner sc=io.FileUtil.openInFile(lexFilename);
		
		sc.nextLine();
		int size=sc.nextInt();
		sc.nextLine();
		String[]dict=new String[size];
		for(int i=0;i<size;i++){
			dict[i]=sc.nextLine();
		}
		phraseLex=buildMap(dict);

		sc.nextLine();
		size=sc.nextInt();
		sc.nextLine();
		dict=new String[size];
		for(int i=0;i<size;i++){
			dict[i]=sc.nextLine();
		}
		wordLex=buildMap(dict);
		sc.close();
	}
	
	private HashMap<String, Integer> buildMap(String[]dict){
		HashMap<String,Integer> map=new HashMap<String,Integer>();
		for(int i=0;i<dict.length;i++){
			map.put(dict[i], i);
		}
		return map;
	}
	
	public void buildList(){
		if(wordList==null){
			wordList=buildList(wordLex);
			phraseList=buildList(phraseLex);
		}
	}
	
	private static String[]buildList(HashMap<String,Integer>lex){
		String dict[]=new String [lex.size()];
		for(String key:lex.keySet()){
			dict[lex.get(key)]=key;
		}
		return dict;
	}
	
	public String getContextString(int context[], boolean addPhraseMarker)
	{
		StringBuffer b = new StringBuffer();
		for (int i=0;i<context.length-1;i++)
		{
			if (b.length() > 0)
				b.append(" ");

			if (i == context.length/2)
				b.append("<PHRASE> ");
			
			b.append(wordList[context[i]]);
		}
		return b.toString();
	}
	
	public static String readLine(BufferedReader r){
		try{
			return r.readLine();
		}
		catch(IOException ioe){
			ioe.printStackTrace();
		}
		return null;
	}
	
}
