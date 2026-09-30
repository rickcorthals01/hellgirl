using System; using System.Collections.Generic; using System.Linq;
// The maze as a graph of 40 px cells (walls from edits.txt), for placing things in it: the Deprived's lairs, glow
// crystals, frozen remains and icicle traps. Cell index = i + j * W; its centre is (X0 + i*P + P/2, Y0 + j*P + P/2).
public static class Cells {
  public const int P = 40, X0 = 40, Y0 = 40, W = 38, H = 26;
  static HashSet<string> seg = new HashSet<string>();
  public static string[] Room = new string[W * H];
  public static void Load(string[] lines) {
    seg.Clear(); Room = new string[W * H];
    foreach (var l in lines) { if (!l.StartsWith("add ")) continue; var v = l.Substring(4).Split(',').Select(int.Parse).ToArray();
      if (v[1] == v[3]) for (int x = Math.Min(v[0], v[2]); x < Math.Max(v[0], v[2]); x += P) seg.Add("h," + x + "," + v[1]);
      else for (int y = Math.Min(v[1], v[3]); y < Math.Max(v[1], v[3]); y += P) seg.Add("v," + v[0] + "," + y); }
  }
  public static void SetRoom(string name, int x0, int y0, int x1, int y1) {
    for (int x = x0; x < x1; x += P) for (int y = y0; y < y1; y += P) Room[Idx(x, y)] = name;
  }
  public static int Idx(int x, int y) { return (x - X0) / P + (y - Y0) / P * W; }
  public static int Cx(int c) { return X0 + c % W * P + P / 2; }
  public static int Cy(int c) { return Y0 + c / W * P + P / 2; }
  // Open neighbours, in the order E, W, S, N (-1 where walled).
  public static int[] Sides(int c) {
    int i = c % W, j = c / W, x = X0 + i * P, y = Y0 + j * P;
    return new[] {
      i + 1 < W && !seg.Contains("v," + (x + P) + "," + y) ? c + 1 : -1,
      i > 0 && !seg.Contains("v," + x + "," + y) ? c - 1 : -1,
      j + 1 < H && !seg.Contains("h," + x + "," + (y + P)) ? c + W : -1,
      j > 0 && !seg.Contains("h," + x + "," + y) ? c - W : -1 };
  }
  public static int Degree(int c) { return Sides(c).Count(n => n >= 0); }
  public static bool Straight(int c) { var s = Sides(c); return Degree(c) == 2 && ((s[0] >= 0 && s[1] >= 0) || (s[2] >= 0 && s[3] >= 0)); }
  public static int[] Bfs(IEnumerable<int> from) {
    var d = Enumerable.Repeat(int.MaxValue, W * H).ToArray(); var q = new Queue<int>();
    foreach (var s in from) { d[s] = 0; q.Enqueue(s); }
    while (q.Count > 0) { int c = q.Dequeue(); foreach (var n in Sides(c)) if (n >= 0 && d[n] == int.MaxValue) { d[n] = d[c] + 1; q.Enqueue(n); } }
    return d;
  }
  // The shortest walk from a cell to the target, as cells.
  public static List<int> Route(int from, int[] toTarget) {
    var path = new List<int> { from }; int c = from;
    while (toTarget[c] > 0) { c = Sides(c).Where(n => n >= 0 && toTarget[n] == toTarget[c] - 1).First(); path.Add(c); }
    return path;
  }
  // Farthest-point picking: each pick is the candidate farthest (by walk) from everything picked so far and from
  // `near` (cells to keep away from), so the picks spread evenly over the maze.
  public static List<int> Spread(IEnumerable<int> candidates, int[] nearDist, int count) {
    var cand = candidates.ToList(); var nearest = (int[])nearDist.Clone(); var picks = new List<int>();
    while (picks.Count < count && cand.Count > 0) {
      int best = cand.OrderByDescending(c => nearest[c]).ThenBy(c => c).First();
      picks.Add(best); cand.Remove(best);
      var d = Bfs(new[] { best }); for (int k = 0; k < nearest.Length; k++) nearest[k] = Math.Min(nearest[k], d[k]);
    }
    return picks;
  }
  // Greedy cover: each pick is the candidate that most shortens the walk from the given cells to their nearest pick
  // (so every corridor has a lair not far off).
  public static List<int> Cover(IEnumerable<int> candidates, int[] cells, int count) {
    var cand = candidates.ToList(); var dist = cand.ToDictionary(c => c, c => Bfs(new[] { c }));
    var nearest = Enumerable.Repeat(200, W * H).ToArray(); var picks = new List<int>();
    while (picks.Count < count && cand.Count > 0) {
      int best = cand.OrderBy(c => cells.Sum(k => (long)Math.Min(nearest[k], dist[c][k]))).ThenBy(c => c).First();
      picks.Add(best); cand.Remove(best);
      for (int k = 0; k < nearest.Length; k++) nearest[k] = Math.Min(nearest[k], dist[best][k]);
    }
    return picks;
  }
  public static int[] Min(int[] a, int[] b) { var r = new int[a.Length]; for (int k = 0; k < a.Length; k++) r[k] = Math.Min(a[k], b[k]); return r; }
}
