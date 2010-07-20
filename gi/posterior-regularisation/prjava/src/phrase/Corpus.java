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

	public Corpus()
	{
		splitSentinel = wordLexicon.insert("<SPLIT>");
		phraseSentinel = wordLexicon.insert("<PHRASE>");		
		rareSentinel = wordLexicon.insert("<RARE>");
	}
	
	public class Edge
	{
		Edge(int phraseId, int contextId, double count)
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
		public String getContextString(boolean insertPhraseSentinel)
		{
			return Corpus.this.getContextString(contextId, insertPhraseSentinel);
		}
		public double getCount()
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
		private double count;
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
		return phraseLexicon.lookup(phraseId);
	}
	
	public String getPhraseString(int phraseId)
	{
		StringBuffer b = new StringBuffer();
		for (int tid: getPhrase(phraseId).toNativeArray())
		{
			if (b.length() > 0)
				b.append(" ");
			b.append(wordLexicon.lookup(tid));
		}
		return b.toString();
	}		
	
	public TIntArrayList getContext(int contextId)
	{
		return contextLexicon.lookup(contextId);
	}
	
	public String getContextString(int contextId, boolean insertPhraseSentinel)
	{
		StringBuffer b = new StringBuffer();
		TIntArrayList c = getContext(contextId);
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
	
	List<Edge> readEdges(Reader in) throws IOException
	{	
		// read in line-by-line
		BufferedReader bin = new BufferedReader(in);
		String line;
		Pattern separator = Pattern.compile(" \\|\\|\\| ");
		
		List<Edge> edges = new ArrayList<Edge>();
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
				ptoks.add(wordLexicon.insert(st.nextToken()));
			int phraseId = phraseLexicon.insert(ptoks);
			
			// process contexts
			String[] parts = separator.split(rest);
			assert (parts.length % 2 == 0);
			for (int i = 0; i < parts.length; i += 2)
			{
				// process pairs of strings - context and count
				String ctxString = parts[i];
				String countString = parts[i + 1];

				assert (countString.startsWith("C="));
				double count = Double.parseDouble(countString.substring(2).trim());
				
				TIntArrayList ctx = new TIntArrayList();
				StringTokenizer ctxStrtok = new StringTokenizer(ctxString, " ");
				while (ctxStrtok.hasMoreTokens())
				{
					String token = ctxStrtok.nextToken();
					ctx.add(wordLexicon.insert(token));
				}
				int contextId = contextLexicon.insert(ctx);

				edges.add(new Edge(phraseId, contextId, count));
			}
		}
		return edges;
	}
	
	static Corpus readFromFile(Reader in) throws IOException
	{	
		Corpus c = new Corpus();
		c.edges = c.readEdges(in);
		for (Edge edge: c.edges)
		{
			while (edge.getPhraseId() >= c.phraseToContext.size())
				c.phraseToContext.add(new ArrayList<Edge>());
			while (edge.getContextId() >= c.contextToPhrase.size())
				c.contextToPhrase.add(new ArrayList<Edge>());
			
			// index the edge for fast phrase, context lookup
			c.phraseToContext.get(edge.getPhraseId()).add(edge);
			c.contextToPhrase.get(edge.getContextId()).add(edge);
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
}