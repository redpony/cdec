import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class Lexicon<T>
{
	public int insert(T word)
	{
		Integer i = wordToIndex.get(word);
		if (i == null)
		{
			i = indexToWord.size();
			wordToIndex.put(word, i);
			indexToWord.add(word);
		}
		return i;
	}

	public T lookup(int index)
	{
		return indexToWord.get(index);
	}

	public int size()
	{
		return indexToWord.size();
	}

	private Map<T, Integer> wordToIndex = new HashMap<T, Integer>();
	private List<T> indexToWord = new ArrayList<T>();
}