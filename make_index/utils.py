def ngrams(string, n=3):
    """ Take a lookup string (noise removed, lower case, etc) and turn into a list of trigrams """

    ngrams = zip(*[string[i:] for i in range(n)])
    return [''.join(ngram) for ngram in ngrams]

def split_list_evenly(data, n):
    if n <= 0 or not data:
        return []

    if n >= len(data):
        return list(range(1, len(data)))

    total_sum = sum(data)
    target_sum = total_sum / n
    current_sum = 0
    split_indices = []

    for i in range(len(data)):
        current_sum += data[i]
        if len(split_indices) < n - 1 and abs(current_sum - target_sum) <= abs(current_sum + data[i+1] - target_sum) if i+1 < len(data) else True:
            if len(split_indices) < n - 1:
                split_indices.append(i + 1)
                current_sum = 0
                target_sum = (total_sum - sum(data[:i+1]))/ (n - len(split_indices)) if len(split_indices) < n else 0

    return split_indices
    
def split_dict_evenly(index_dict, num_parts):
    flat_list = [ [x, index_dict[x] ] for x in sorted(index_dict) ]
    flat_values = [ index_dict[x] for x in sorted(index_dict) ]

    indexes = split_list_evenly(flat_values, num_parts)
    split_flat_list = [ dict() for i in range(num_parts) ]
    current = 0
    for i, (k, v) in enumerate(flat_list):
        try:
            if i >= indexes[current]:
                current += 1
        except IndexError:
            pass
        split_flat_list[current][k] = v

    return split_flat_list
