package test;

import java.util.Arrays;
import java.util.HashMap;

import data.Corpus;
import hmm.POS;

public class CorpusTest {

	public static void main(String[] args) {
		Corpus c=new Corpus(POS.trainFilename);

		
		int idx=30;
		
		
		HashMap<String, Integer>vocab=
			(HashMap<String, Integer>) io.SerializedObjects.readSerializedObject(Corpus.alphaFilename);
		
		HashMap<String, Integer>tagVocab=
			(HashMap<String, Integer>) io.SerializedObjects.readSerializedObject(Corpus.tagalphaFilename);
		
		
		String [] dict=new String [vocab.size()+1];
		for(String key:vocab.keySet()){
			dict[vocab.get(key)]=key;
		}
		dict[dict.length-1]=Corpus.UNK_TOK;
		
		String [] tagdict=new String [tagVocab.size()+1];
		for(String key:tagVocab.keySet()){
			tagdict[tagVocab.get(key)]=key;
		}
		tagdict[tagdict.length-1]=Corpus.UNK_TOK;
		
		String[] sent=c.get(idx);
		int []data=c.getInt(idx);
		
		
		String []roundtrip=new String [sent.length];
		for(int i=0;i<sent.length;i++){
			roundtrip[i]=dict[data[i]];
		}
		System.out.println(Arrays.toString(sent));
		System.out.println(Arrays.toString(roundtrip));
		
		sent=c.tag.get(idx);
		data=c.tagData.get(idx);
		
		
		roundtrip=new String [sent.length];
		for(int i=0;i<sent.length;i++){
			roundtrip[i]=tagdict[data[i]];
		}
		System.out.println(Arrays.toString(sent));
		System.out.println(Arrays.toString(roundtrip));
	}

}
