package util;

import java.util.Comparator;

public class Sorters {
	public static class sortWordsCounts implements Comparator{
		
		/**
		 * Sorter for a pair of word id, counts. Sort ascending by counts
		 */
		public int compare(Object arg0, Object arg1) {
			Pair<Integer,Integer> p1 = (Pair<Integer,Integer>)arg0;
			Pair<Integer,Integer> p2 = (Pair<Integer,Integer>)arg1;
			if(p1.second() > p2.second()){
				return 1;
			}else{
				return -1;
			}
		}
		
	}
	
public static class sortWordsDouble implements Comparator{
		
		/**
		 * Sorter for a pair of word id, counts. Sort by counts
		 */
		public int compare(Object arg0, Object arg1) {
			Pair<Integer,Double> p1 = (Pair<Integer,Double>)arg0;
			Pair<Integer,Double> p2 = (Pair<Integer,Double>)arg1;
			if(p1.second() < p2.second()){
				return 1;
			}else{
				return -1;
			}
		}
		
	}
}
