import gnu.trove.TIntArrayList;

import java.io.*;
import java.util.*;
import java.util.regex.Pattern;

public class Corpus
{
	private Lexicon<String> tokenLexicon = new Lexicon<String>();
	private Lexicon<TIntArrayList> phraseLexicon = new Lexicon<TIntArrayList>();
	private Lexicon<TIntArrayList> contextLexicon = new Lexicon<TIntArrayList>();
	private List<Edge> edges = new ArrayList<Edge>();
	private List<List<Edge>> phraseToContext = new ArrayList<List<Edge>>();
	private List<List<Edge>> contextToPhrase = new ArrayList<List<Edge>>();
	
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
			return phraseLexicon.lookup(phraseId);
		}
		public String getPhraseString()
		{
			StringBuffer b = new StringBuffer();
			for (int tid: getPhrase().toNativeArray())
			{
				if (b.length() > 0)
					b.append(" ");
				b.append(tokenLexicon.lookup(tid));
			}
			return b.toString();
		}		
		public int getContextId()
		{
			return contextId;
		}
		public TIntArrayList getContext()
		{
			return contextLexicon.lookup(contextId);
		}
		public String getContextString()
		{
			StringBuffer b = new StringBuffer();
			for (int tid: getContext().toNativeArray())
			{
				if (b.length() > 0)
					b.append(" ");
				b.append(tokenLexicon.lookup(tid));
			}
			return b.toString();
		}
		public int getCount()
		{
			return count;
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
	
	int getNumTokens()
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
					if (!token.equals("<PHRASE>"))
						ctx.add(c.tokenLexicon.insert(token));
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
}
