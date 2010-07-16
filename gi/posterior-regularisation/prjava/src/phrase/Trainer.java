package phrase;

import io.FileUtil;
import joptsimple.OptionParser;
import joptsimple.OptionSet;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.PrintStream;
import java.util.Random;

import arr.F;

public class Trainer 
{
	public static void main(String[] args) 
	{
        OptionParser parser = new OptionParser();
        parser.accepts("help");
        parser.accepts("in").withRequiredArg().ofType(File.class);
        parser.accepts("out").withRequiredArg().ofType(File.class);
        parser.accepts("start").withRequiredArg().ofType(File.class);
        parser.accepts("parameters").withRequiredArg().ofType(File.class);
        parser.accepts("topics").withRequiredArg().ofType(Integer.class).defaultsTo(5);
        parser.accepts("iterations").withRequiredArg().ofType(Integer.class).defaultsTo(10);
        parser.accepts("threads").withRequiredArg().ofType(Integer.class).defaultsTo(0);
        parser.accepts("scale-phrase").withRequiredArg().ofType(Double.class).defaultsTo(0.0);
        parser.accepts("scale-context").withRequiredArg().ofType(Double.class).defaultsTo(0.0);
        parser.accepts("seed").withRequiredArg().ofType(Long.class).defaultsTo(0l);
        parser.accepts("convergence-threshold").withRequiredArg().ofType(Double.class).defaultsTo(1e-6);
        parser.accepts("variational-bayes");
        parser.accepts("alpha-emit").withRequiredArg().ofType(Double.class).defaultsTo(0.1);
        parser.accepts("alpha-pi").withRequiredArg().ofType(Double.class).defaultsTo(0.01);
        parser.accepts("agree");
        parser.accepts("no-parameter-cache");
        parser.accepts("skip-large-phrases").withRequiredArg().ofType(Integer.class).defaultsTo(5);
        parser.accepts("rare-word").withRequiredArg().ofType(Integer.class).defaultsTo(0);
        parser.accepts("rare-edge").withRequiredArg().ofType(Integer.class).defaultsTo(0);
        OptionSet options = parser.parse(args);

        if (options.has("help") || !options.has("in"))
        {
        	try {
				parser.printHelpOn(System.err);
			} catch (IOException e) {
				System.err.println("This should never happen.");
				e.printStackTrace();
			}
        	System.exit(1);     
        }
		
		int tags = (Integer) options.valueOf("topics");
		int iterations = (Integer) options.valueOf("iterations");
		double scale_phrase = (Double) options.valueOf("scale-phrase");
		double scale_context = (Double) options.valueOf("scale-context");
		int threads = (Integer) options.valueOf("threads");
		double threshold = (Double) options.valueOf("convergence-threshold");
		boolean vb = options.has("variational-bayes");
		double alphaEmit = (vb) ? (Double) options.valueOf("alpha-emit") : 0;
		double alphaPi = (vb) ? (Double) options.valueOf("alpha-pi") : 0;
		int skip = (Integer) options.valueOf("skip-large-phrases");
		int wordThreshold = (Integer) options.valueOf("rare-word");
		int edgeThreshold = (Integer) options.valueOf("rare-edge");
		
		if (options.has("seed"))
			F.rng = new Random((Long) options.valueOf("seed"));
		
		if (tags <= 1 || scale_phrase < 0 || scale_context < 0 || threshold < 0)
		{
			System.err.println("Invalid arguments. Try again!");
			System.exit(1);
		}
		
		Corpus corpus = null;
		File infile = (File) options.valueOf("in");
		try {
			System.out.println("Reading concordance from " + infile);
			corpus = Corpus.readFromFile(FileUtil.reader(infile));
			corpus.printStats(System.out);
		} catch (IOException e) {
			System.err.println("Failed to open input file: " + infile);
			e.printStackTrace();
			System.exit(1);
		}
		
		if (wordThreshold > 0)
			corpus.applyWordThreshold(wordThreshold);
		
		if (!options.has("agree"))
			System.out.println("Running with " + tags + " tags " +
					"for " + iterations + " iterations " +
					((skip > 0) ? "skipping large phrases for first " + skip + " iterations " : "") +
					"with scale " + scale_phrase + " phrase and " + scale_context + " context " +
					"and " + threads + " threads");
		else
			System.out.println("Running agreement model with " + tags + " tags " +
	 				"for " + iterations);

	 	System.out.println();
		
 		PhraseCluster cluster = null;
 		Agree agree = null;
 		if (options.has("agree"))
 			agree = new Agree(tags, corpus);
 		else
 		{
 			cluster = new PhraseCluster(tags, corpus);
 			if (threads > 0) cluster.useThreadPool(threads);
 			if (vb)	cluster.initialiseVB(alphaEmit, alphaPi);
 			if (options.has("no-parameter-cache")) 
 				cluster.cacheLambda = false;
 			if (options.has("start"))
 			{
 				try {
					System.err.println("Reading starting parameters from " + options.valueOf("start"));
					cluster.loadParameters(FileUtil.reader((File)options.valueOf("start")));
				} catch (IOException e) {
					System.err.println("Failed to open input file: " + options.valueOf("start"));
					e.printStackTrace();
				}
 			}
			cluster.setEdgeThreshold(edgeThreshold);
 		}
				
		double last = 0;
		for (int i=0; i < iterations; i++)
		{
			double o;
			if (agree != null)
				o = agree.EM();
			else
			{
				if (scale_phrase <= 0 && scale_context <= 0)
				{
					if (!vb)
						o = cluster.EM(i < skip);
					else
						o = cluster.VBEM(alphaEmit, alphaPi, i < skip);	
				}
				else
					o = cluster.PREM(scale_phrase, scale_context, i < skip);
			}
			
			System.out.println("ITER: "+i+" objective: " + o);
			
			if (i != 0 && Math.abs((o - last) / o) < threshold)
			{
				last = o;
				break;
			}
			last = o;
		}
		
		if (cluster == null)
			cluster = agree.model1;

		double pl1lmax = cluster.phrase_l1lmax();
		double cl1lmax = cluster.context_l1lmax();
		System.out.println("\nFinal posterior phrase l1lmax " + pl1lmax + " context l1lmax " + cl1lmax);
		
		if (options.has("out"))
		{
			File outfile = (File) options.valueOf("out");
			try {
				PrintStream ps = FileUtil.printstream(outfile);
				cluster.displayPosterior(ps);
				ps.close();
			} catch (IOException e) {
				System.err.println("Failed to open output file: " + outfile);
				e.printStackTrace();
				System.exit(1);
			}
		}

		if (options.has("parameters"))
		{
			File outfile = (File) options.valueOf("parameters");
			PrintStream ps;
			try {
				ps = FileUtil.printstream(outfile);
				cluster.displayModelParam(ps);
				ps.close();
			} catch (IOException e) {
				System.err.println("Failed to open output parameters file: " + outfile);
				e.printStackTrace();
				System.exit(1);
			}
		}
		
		if (cluster.pool != null)
			cluster.pool.shutdown();
	}
}
