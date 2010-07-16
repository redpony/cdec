package phrase;

import gnu.trove.TIntArrayList;

import java.io.*;
import java.util.*;
import java.util.regex.Pattern;


public class Corpus
{
	private Lexicon<String> wordLexicon = new Lexicon<String>();
	private Lexicon<TIntArrayList> phraseLexicon = new Lexicon<TIntArrayList>();
	private Lexicon<TIntArrayList> contextLexicon = new Lexicon<TIntArrayList>();
	private List<Edge> edges = new ArrayList<Edge>();
	private List<List<Edge>> phraseToContext = new ArrayList<List<Edge>>();
	private List<List<Edge>> contextToPhrase = new ArrayList<List<Edge>>();
	public int splitSentinel;
	public int phraseSentinel;
	public int rareSentinel;
	private boolean[] wordIsRare;

	public Corpus()
	{
		splitSentinel = wordLexicon.insert("<SPLIT>");
		phraseSentinel = wordLexicon.insert("<PHRASE>");		
		rareSentinel = wordLexicon.insert("<RARE>");
	}
	
	public class Edge
	{
		Edge(int phraseId, int contextId, int count)
		{
			this.phraseId = phraseId;
			this.contextId = contextId;
			this.count = count;
		}
		public int getPhraseId()
		{
			return phraseId;
		}
		public TIntArrayList getPhrase()
		{
			return Corpus.this.getPhrase(phraseId);
		}
		public TIntArrayList getRawPhrase()
		{
			return Corpus.this.getRawPhrase(phraseId);
		}
		public String getPhraseString()
		{
			return Corpus.this.getPhraseString(phraseId);
		}		
		public int getContextId()
		{
			return contextId;
		}
		public TIntArrayList getContext()
		{
			return Corpus.this.getContext(contextId);
		}
		public TIntArrayList getRawContext()
		{
			return Corpus.this.getRawContext(contextId);
		}		public String getContextString(boolean insertPhraseSentinel)
		{
			return Corpus.this.getContextString(contextId, insertPhraseSentinel);
		}
		public int getCount()
		{
			return count;
		}
		public boolean equals(Object other)
		{
			if (other instanceof Edge) 
			{
				Edge oe = (Edge) other;
				return oe.phraseId == phraseId && oe.contextId == contextId; 
			}
			else return false;
		}
		public int hashCode()
		{   // this is how boost's hash_combine does it
			int seed = phraseId;
			seed ^= contextId + 0x9e3779b9 + (seed << 6) + (seed >> 2);
			return seed;
		}
		public String toString()
		{
			return getPhraseString() + "\t" + getContextString(true);
		}
		
		private int phraseId;
		private int contextId;
		private int count;
	}

	List<Edge> getEdges()
	{
		return edges;
	}
	
	int getNumEdges()
	{
		return edges.size();
	}

	int getNumPhrases()
	{
		return phraseLexicon.size();
	}
	
	int getNumContextPositions()
	{
		return contextLexicon.lookup(0).size();
	}
	
	List<Edge> getEdgesForPhrase(int phraseId)
	{
		return phraseToContext.get(phraseId);
	}
	
	int getNumContexts()
	{
		return contextLexicon.size();
	}
	
	List<Edge> getEdgesForContext(int contextId)
	{
		return contextToPhrase.get(contextId);
	}
	
	int getNumWords()
	{
		return wordLexicon.size();
	}
	
	String getWord(int wordId)
	{
		return wordLexicon.lookup(wordId);
	}
	
	public TIntArrayList getPhrase(int phraseId)
	{
		TIntArrayList phrase = phraseLexicon.lookup(phraseId);
		if (wordIsRare != null)
		{
			boolean first = true;
			for (int i = 0; i < phrase.size(); ++i)
			{
				if (wordIsRare[phrase.get(i)])
				{
					if (first)
					{
						phrase = (TIntArrayList) phrase.clone();
						first = false;
					}
					phrase.set(i, rareSentinel);
				}
			}
		}
		return phrase;
	}
	
	public TIntArrayList getRawPhrase(int phraseId)
	{
		return phraseLexicon.lookup(phraseId);
	}
	
	public String getPhraseString(int phraseId)
	{
		StringBuffer b = new StringBuffer();
		for (int tid: getRawPhrase(phraseId).toNativeArray())
		{
			if (b.length() > 0)
				b.append(" ");
			b.append(wordLexicon.lookup(tid));
		}
		return b.toString();
	}		
	
	public TIntArrayList getContext(int contextId)
	{
		TIntArrayList context = contextLexicon.lookup(contextId);
		if (wordIsRare != null)
		{
			boolean first = true;
			for (int i = 0; i < context.size(); ++i)
			{
				if (wordIsRare[context.get(i)])
				{
					if (first)
					{
						context = (TIntArrayList) context.clone();
						first = false;
					}
					context.set(i, rareSentinel);
				}
			}
		}
		return context;
	}
	
	public TIntArrayList getRawContext(int contextId)
	{
		return contextLexicon.lookup(contextId);
	}
	
	public String getContextString(int contextId, boolean insertPhraseSentinel)
	{
		StringBuffer b = new StringBuffer();
		TIntArrayList c = getRawContext(contextId);
		for (int i = 0; i < c.size(); ++i)
		{
			if (i > 0) b.append(" ");
			//if (i == c.size() / 2) b.append("<PHRASE> ");
			b.append(wordLexicon.lookup(c.get(i)));
		}
		return b.toString();
	}
	
	public boolean isSentinel(int wordId)
	{
		return wordId == splitSentinel || wordId == phraseSentinel;
	}
	
	static Corpus readFromFile(Reader in) throws IOException
	{
		Corpus c = new Corpus();
		
		// read in line-by-line
		BufferedReader bin = new BufferedReader(in);
		String line;
		Pattern separator = Pattern.compile(" \\|\\|\\| ");

		while ((line = bin.readLine()) != null)
		{
			// split into phrase and contexts
			StringTokenizer st = new StringTokenizer(line, "\t");
			assert (st.hasMoreTokens());
			String phraseToks = st.nextToken();
			assert (st.hasMoreTokens());
			String rest = st.nextToken();
			assert (!st.hasMoreTokens());

			// process phrase	
			st = new StringTokenizer(phraseToks, " ");
			TIntArrayList ptoks = new TIntArrayList();
			while (st.hasMoreTokens())
				ptoks.add(c.wordLexicon.insert(st.nextToken()));
			int phraseId = c.phraseLexicon.insert(ptoks);
			if (phraseId == c.phraseToContext.size())
				c.phraseToContext.add(new ArrayList<Edge>());
			
			// process contexts
			String[] parts = separator.split(rest);
			assert (parts.length % 2 == 0);
			for (int i = 0; i < parts.length; i += 2)
			{
				// process pairs of strings - context and count
				TIntArrayList ctx = new TIntArrayList();
				String ctxString = parts[i];
				String countString = parts[i + 1];
				StringTokenizer ctxStrtok = new StringTokenizer(ctxString, " ");
				while (ctxStrtok.hasMoreTokens())
				{
					String token = ctxStrtok.nextToken();
					//if (!token.equals("<PHRASE>"))
						ctx.add(c.wordLexicon.insert(token));
				}
				int contextId = c.contextLexicon.insert(ctx);
				if (contextId == c.contextToPhrase.size())
					c.contextToPhrase.add(new ArrayList<Edge>());

				assert (countString.startsWith("C="));
				Edge e = c.new Edge(phraseId, contextId, 
						Integer.parseInt(countString.substring(2).trim()));
				c.edges.add(e);
				
				// index the edge for fast phrase, context lookup
				c.phraseToContext.get(phraseId).add(e);
				c.contextToPhrase.get(contextId).add(e);
			}
		}
		
		return c;
	}
	
	TIntArrayList phraseEdges(TIntArrayList phrase)
	{
		TIntArrayList r = new TIntArrayList(4);
		for (int p = 0; p < phrase.size(); ++p)
		{
			if (p == 0 || phrase.get(p-1) == splitSentinel) 				
				r.add(p);
			if (p == phrase.size() - 1 || phrase.get(p+1) == splitSentinel) 
				r.add(p);
		}
		return r;
	}

	public void printStats(PrintStream out) 
	{
		out.println("Corpus has " + edges.size() + " edges " + phraseLexicon.size() + " phrases " 
				+ contextLexicon.size() + " contexts and " + wordLexicon.size() + " word types");
	}

	public void applyWordThreshold(int wordThreshold) 
	{
		int[] counts = new int[wordLexicon.size()];
		for (Edge e: edges)
		{
			TIntArrayList phrase = e.getPhrase();
			for (int i = 0; i < phrase.size(); ++i)
				counts[phrase.get(i)] += e.getCount();
			
			TIntArrayList context = e.getContext();
			for (int i = 0; i < context.size(); ++i)
				counts[context.get(i)] += e.getCount();
		}

		wordIsRare = new boolean[wordLexicon.size()];
		for (int i = 0; i < wordLexicon.size(); ++i)
			wordIsRare[i] = counts[i] < wordThreshold;
	}
}