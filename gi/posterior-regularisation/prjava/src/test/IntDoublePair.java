package test;

public class IntDoublePair implements Comparable{
	double val;
	int idx;
	public int compareTo(Object o){
		if(o instanceof IntDoublePair){
			IntDoublePair pair=(IntDoublePair)o;
			if(pair.val>val){
				return 1;
			}
			if(pair.val<val){
				return -1;
			}
			return 0;
		}
		return -1;
	}
	public IntDoublePair(int i,double v){
		val=v;
		idx=i;
	}
}
