using System; using System.Collections.Generic;
// Walk checks on an occupancy grid: where a body with some clearance fits, shortest routes, connected regions.
public static class Walk {
  public static bool[,] Walkable(bool[,] wall, int clear) {
    int W=wall.GetLength(0), H=wall.GetLength(1); var ok=new bool[W,H];
    for (int y=0;y<H;y++) for (int x=0;x<W;x++) { bool free=true;
      for (int dy=-clear;dy<=clear&&free;dy++) for (int dx=-clear;dx<=clear&&free;dx++) { int X=x+dx,Y=y+dy; if (X<0||Y<0||X>=W||Y>=H||wall[X,Y]) free=false; }
      ok[x,y]=free; }
    return ok;
  }
  public static int[] Route(bool[,] ok, int sx, int sy, int tx, int ty, int tr) {
    int W=ok.GetLength(0), H=ok.GetLength(1); var prev=new int[W*H]; for (int i=0;i<prev.Length;i++) prev[i]=-2;
    var q=new Queue<int>(); int s=sy*W+sx; if (!ok[sx,sy]) return new int[0]; prev[s]=-1; q.Enqueue(s);
    while (q.Count>0) { int i=q.Dequeue(); int x=i%W,y=i/W;
      if ((x-tx)*(x-tx)+(y-ty)*(y-ty)<=tr*tr) { var path=new List<int>(); for (int j=i;j!=-1;j=prev[j]) path.Add(j); path.Reverse(); return path.ToArray(); }
      int[] nx={x+1,x-1,x,x}, ny={y,y,y+1,y-1};
      for (int k=0;k<4;k++) { int X=nx[k],Y=ny[k]; if (X<0||Y<0||X>=W||Y>=H||!ok[X,Y]) continue; int j=Y*W+X; if (prev[j]!=-2) continue; prev[j]=i; q.Enqueue(j); } }
    return new int[0];
  }
  // Region id per walkable cell (0 = not walkable).
  public static int[,] Regions(bool[,] ok, out int count) {
    int W=ok.GetLength(0), H=ok.GetLength(1); var id=new int[W,H]; count=0;
    for (int y=0;y<H;y++) for (int x=0;x<W;x++) if (ok[x,y]&&id[x,y]==0) { count++; var q=new Queue<int>(); q.Enqueue(y*W+x); id[x,y]=count;
      while (q.Count>0) { int i=q.Dequeue(); int cx=i%W, cy=i/W; int[] nx={cx+1,cx-1,cx,cx}, ny={cy,cy,cy+1,cy-1};
        for (int k=0;k<4;k++) { int X=nx[k],Y=ny[k]; if (X<0||Y<0||X>=W||Y>=H||!ok[X,Y]||id[X,Y]!=0) continue; id[X,Y]=count; q.Enqueue(Y*W+X); } } }
    return id;
  }
}
