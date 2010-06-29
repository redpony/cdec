import gnu.trove.TIntArrayList;

import java.io.*;
import java.util.*;
import java.util.regex.Pattern;

public class Corpus
{
	private Lexicon<String> tokenLexicon = new Lexicon<String>();
	private Lexicon<TIntArrayList> ngramLexicon = new Lexicon<TIntArrayList>();
	private List<Edge> edges = new ArrayList<Edge>();
	private Map<Ngram,List<Edge>> phraseToContext = new HashMap<Ngram,List<Edge>>();
	private Map<Ngram,List<Edge>> contextToPhrase = new HashMap<Ngram,List<Edge>>();
	
	public class Ngram
	{
		private Ngram(int id)
		{
			ngramId = id;
		}
		public int getId()
		{
			return ngramId;
		}
		public TIntArrayList getTokenIds()
		{
			return ngramLexicon.lookup(ngramId);
		}
		public String toString()
		{
			StringBuffer b = new StringBuffer();
			for (int tid: getTokenIds().toNativeArray())
			{
				if (b.length() > 0)
					b.append(" ");
				b.append(tokenLexicon.lookup(tid));
			}
			return b.toString();
		}
		public int hashCode()
		{
			return ngramId;
		}
		public boolean equals(Object other)
		{
			return other instanceof Ngram && ngramId == ((Ngram) other).ngramId;
		}
		private int ngramId;
	}
	
	public class Edge
	{
		Edge(Ngram phrase, Ngram context, int count)
		{
			this.phrase = phrase;
			this.context = context;
			this.count = count;
		}
		public Ngram getPhrase()
		{
			return phrase;
		}
		public Ngram getContext()
		{
			return context;
		}
		public int getCount()
		{
			return count;
		}
		private Ngram phrase;
		private Ngram context;
		private int count;
	}

	List<Edge> getEdges()
	{
		return edges;
	}
	
	int numEdges()
	{
		return edges.size();
	}

	Set<Ngram> getPhrases()
	{
		return phraseToContext.keySet();
	}
	
	List<Edge> getEdgesForPhrase(Ngram phrase)
	{
		return phraseToContext.get(phrase);
	}
	
	Set<Ngram> getContexts()
	{
		return contextToPhrase.keySet();
	}
	
	List<Edge> getEdgesForContext(Ngram context)
	{
		return contextToPhrase.get(context);
	}
	
	int numTokens()
	{
		return tokenLexicon.size();
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
				ptoks.add(c.tokenLexicon.insert(st.nextToken()));
			int phraseId = c.ngramLexicon.insert(ptoks);
			Ngram phrase = c.new Ngram(phraseId);
			
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
					if (!token.equals("<PHRASE>"))
						ctx.add(c.tokenLexicon.insert(token));
				}
				int contextId = c.ngramLexicon.insert(ctx);
				Ngram context = c.new Ngram(contextId);

				assert (countString.startsWith("C="));
				Edge e = c.new Edge(phrase, context, Integer.parseInt(countString.substring(2).trim()));
				c.edges.add(e);
				
				// index the edge for fast phrase lookup
				List<Edge> edges = c.phraseToContext.get(phrase);
				if (edges == null)
				{
					edges = new ArrayList<Edge>();
					c.phraseToContext.put(phrase, edges);
				}
				edges.add(e);
				
				// index the edge for fast context lookup
				edges = c.contextToPhrase.get(context);
				if (edges == null)
				{
					edges = new ArrayList<Edge>();
					c.contextToPhrase.put(context, edges);
				}
				edges.add(e);
			}
		}
		
		return c;
	}	
}
