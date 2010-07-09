package util;

public class Pair<O1, O2> {
	public O1 _first;
	public O2 _second;

	public final O1 first() {
		return _first;
	}

	public final O2 second() {
		return _second;
	}

	public final void setFirst(O1 value){
		_first = value;
	}
	
	public final void setSecond(O2 value){
		_second = value;
	}
	
	public Pair(O1 first, O2 second) {
		_first = first;
		_second = second;
	}

	public String toString(){
		return _first + " " + _second; 
	}
}
