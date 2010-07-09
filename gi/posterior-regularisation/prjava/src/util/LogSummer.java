package util;

import java.lang.Math;

/*
 * Math tool for computing logs of sums, when the terms of the sum are already in log form.
 * (Useful if the terms of the sum are very small numbers.)
 */
public class LogSummer {
	
	private LogSummer() {
	}
		
	/**
	 * Given log(a) and log(b), computes log(a + b).
	 * 
	 * @param  loga log of first sum term
	 * @param  logb log of second sum term
	 * @return     log(sum), where sum = a + b
	 */
	public static double sum(double loga, double logb) {
		assert(!Double.isNaN(loga));
		assert(!Double.isNaN(logb));
		
		if(Double.isInfinite(loga))
			return logb;
		if(Double.isInfinite(logb))
			return loga;

		double maxLog;
		double difference;
		if(loga > logb) {
			difference = logb - loga;
			maxLog = loga;
		}
		else {
			difference = loga - logb;
			maxLog = logb;
		}

		return Math.log1p(Math.exp(difference)) + maxLog;
	}

	/**
	 * Computes log(exp(array[index]) + b), and
	 * modifies array[index] to contain this new value.
	 * 
	 * @param array array to modify
	 * @param index index at which to modify
	 * @param logb  log of the second sum term
	 */
	public static void sum(double[] array, int index, double logb) {
		array[index] = sum(array[index], logb);
	}
	
	/**
	 * Computes log(a + b + c + ...) from log(a), log(b), log(c), ...
	 * by recursively splitting the input and delegating to the sum method.
	 * 
	 * @param  terms an array containing the log of all the terms for the sum
	 * @return log(sum), where sum = exp(terms[0]) + exp(terms[1]) + ...
	 */
	public static double sumAll(double... terms) {
		return sumAllHelper(terms, 0, terms.length);
	}
	
	/**
	 * Computes log(a_0 + a_1 + ...) from a_0 = exp(terms[begin]),
	 * a_1 = exp(terms[begin + 1]), ..., a_{end - 1 - begin} = exp(terms[end - 1]).
	 * 
	 * @param  terms an array containing the log of all the terms for the sum,
	 *               and possibly some other terms that will not go into the sum
	 * @return log of the sum of the elements in the [begin, end) region of the terms array
	 */
	private static double sumAllHelper(final double[] terms, final int begin, final int end) {
		int length = end - begin;
		switch(length) {
			case 0: return Double.NEGATIVE_INFINITY;
			case 1: return terms[begin];
			default:
				int midIndex = begin + length/2;
				return sum(sumAllHelper(terms, begin, midIndex), sumAllHelper(terms, midIndex, end));
		}
	}

}