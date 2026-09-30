using System; using System.Drawing; using System.Drawing.Imaging; using System.Collections.Generic; using System.IO; using System.Linq;

// Cleans the maze sketch into straight walls on a lattice of L sketch pixels.
// Walls are lattice edges: H(i,j) runs from (i,j) to (i+1,j); V(i,j) from (i,j) to (i,j+1) (vertex (i,j) = pixel (i*L, j*L)).
public static class Snap {
  public static int L = 20;
  public static int NX, NY; // vertices
  public static bool[,] H, V; // H[i,j]: i < NX-1; V[i,j]: j < NY-1
  public static bool[,] Ink; public static int IW, IH; // the sketch's black strokes

  // An empty lattice (walls come from edits only).
  public static void Empty(int w, int h) {
    NX = w/L + 2; NY = h/L + 2; H = new bool[NX,NY]; V = new bool[NX,NY];
    Ink = new bool[w,h]; IW = w; IH = h;
  }

  public static void FromSketch(string path) {
    var bmp = new Bitmap(path); int w = bmp.Width, h = bmp.Height;
    var data = bmp.LockBits(new Rectangle(0,0,w,h), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
    int[] px = new int[w*h]; System.Runtime.InteropServices.Marshal.Copy(data.Scan0, px, 0, w*h); bmp.UnlockBits(data);
    var black = new bool[w,h]; Ink = black; IW = w; IH = h;
    for (int y=0;y<h;y++) for (int x=0;x<w;x++) { int c=px[y*w+x]; int r=(c>>16)&255,g=(c>>8)&255,b=c&255; black[x,y]=r<110&&g<110&&b<110; }
    int[,] dots = {{153,82},{1487,70},{147,1002},{1490,972}};
    for (int k=0;k<4;k++) for (int y=-30;y<=30;y++) for (int x=-30;x<=30;x++) { int X=dots[k,0]+x,Y=dots[k,1]+y; if (X>=0&&Y>=0&&X<w&&Y<h&&x*x+y*y<=900) black[X,Y]=false; }
    for (int y=385;y<=560;y++) for (int x=690;x<=895;x++) black[x,y]=false; // the vortex spiral
    NX = w/L + 2; NY = h/L + 2;
    var hv = new int[NX,NY]; var vv = new int[NX,NY];
    for (int y=0;y<h;y++) for (int x=0;x<w;x++) {
      if (!black[x,y]) continue;
      int hr=1; for (int d=1; d<25 && x+d<w && black[x+d,y]; d++) hr++; for (int d=1; d<25 && x-d>=0 && black[x-d,y]; d++) hr++;
      int vr=1; for (int d=1; d<25 && y+d<h && black[x,y+d]; d++) vr++; for (int d=1; d<25 && y-d>=0 && black[x,y-d]; d++) vr++;
      if (hr >= vr) hv[x/L, (y+L/2)/L]++; else vv[(x+L/2)/L, y/L]++;
    }
    H = new bool[NX,NY]; V = new bool[NX,NY];
    for (int i=0;i<NX;i++) for (int j=0;j<NY;j++) { H[i,j] = hv[i,j] >= L*11/10; V[i,j] = vv[i,j] >= L*11/10; }
    Straighten(); Straighten();
    CloseGaps();
  }

  // A short run (<= 3 edges) that touches a longer run end to end one row (or column) over moves onto that row.
  static void Straighten() {
    for (int pass=0; pass<2; pass++) {
      bool hor = pass==0; var E = hor?H:V; int A = hor?NX:NY, B = hor?NY:NX;
      for (int b=1;b<B-1;b++) {
        int a=0;
        while (a<A) {
          if (!Get(E,hor,a,b)) { a++; continue; }
          int s=a; while (a<A && Get(E,hor,a,b)) a++; int len=a-s; // run [s,a)
          if (len>Math.Max(1,60/L)) continue;
          foreach (int nb in new[]{b-1,b+1}) {
            int left = RunLen(E,hor,s-1,nb,-1), right = RunLen(E,hor,a,nb,1);
            if (Math.Max(left,right) > len) { for (int k=s;k<a;k++) { Set(E,hor,k,b,false); Set(E,hor,k,nb,true); } break; }
          }
        }
      }
    }
  }
  static int RunLen(bool[,] E, bool hor, int a, int b, int step) { int n=0; while (a>=0 && a<(hor?NX:NY) && Get(E,hor,a,b)) { n++; a+=step; } return n; }
  static bool Get(bool[,] E, bool hor, int a, int b) { if (a<0||b<0) return false; return hor ? (a<NX&&b<NY&&E[a,b]) : (b<NX&&a<NY&&E[b,a]); }
  static void Set(bool[,] E, bool hor, int a, int b, bool v) { if (a<0||b<0) return; if (hor) { if (a<NX&&b<NY) E[a,b]=v; } else { if (b<NX&&a<NY) E[b,a]=v; } }

  public static bool VertexUsed(int i, int j) {
    return (i>0&&H[i-1,j]) || H[i,j] || (j>0&&V[i,j-1]) || V[i,j];
  }
  // One-edge gaps: between two collinear walls, or between a wall's end and another wall.
  static void CloseGaps() {
    for (int j=0;j<NY;j++) for (int i=1;i<NX-2;i++) if (!H[i,j] && H[i-1,j] && H[i+1,j] && Coverage(true,i,j)>=0.4) H[i,j]=true;
    for (int i=0;i<NX;i++) for (int j=1;j<NY-2;j++) if (!V[i,j] && V[i,j-1] && V[i,j+1] && Coverage(false,i,j)>=0.4) V[i,j]=true;
    var addH = new List<int[]>(); var addV = new List<int[]>();
    for (int j=0;j<NY;j++) for (int i=0;i<NX-1;i++) if (!H[i,j]) {
      bool leftEnd = i>0 && H[i-1,j] && !(i>1 && false), rightEnd = i+1<NX-1 && H[i+1,j];
      // wall ends at vertex i and a wall passes through vertex i+1 (or the mirror).
      if (leftEnd && !rightEnd && VertexHasV(i+1,j) && !VertexHasV(i,j)) addH.Add(new[]{i,j});
      if (rightEnd && !leftEnd && VertexHasV(i,j) && !VertexHasV(i+1,j)) addH.Add(new[]{i,j});
    }
    for (int i=0;i<NX;i++) for (int j=0;j<NY-1;j++) if (!V[i,j]) {
      bool upEnd = j>0 && V[i,j-1], downEnd = j+1<NY-1 && V[i,j+1];
      if (upEnd && !downEnd && VertexHasH(i,j+1) && !VertexHasH(i,j)) addV.Add(new[]{i,j});
      if (downEnd && !upEnd && VertexHasH(i,j) && !VertexHasH(i,j+1)) addV.Add(new[]{i,j});
    }
    foreach (var e in addH) if (Coverage(true,e[0],e[1])>=0.3) H[e[0],e[1]]=true;
    foreach (var e in addV) if (Coverage(false,e[0],e[1])>=0.3) V[e[0],e[1]]=true;
  }
  static bool VertexHasV(int i, int j) { return (j>0&&V[i,j-1]) || (j<NY&&V[i,j]); }
  static bool VertexHasH(int i, int j) { return (i>0&&H[i-1,j]) || (i<NX&&H[i,j]); }

  // Doorways all one size: along every lattice line, a gap of 2..maxGap edges between two solid points (a wall on the
  // line, or a wall crossing it) is narrowed to one edge in its middle. Wider gaps are open room sides and stay.
  public static int NormalizeGaps(int maxGap) {
    int changed=0;
    for (int pass=0; pass<2; pass++) { bool hor = pass==0; int A = hor?NX:NY, B = hor?NY:NX; var E = hor?H:V;
      for (int b=0;b<B;b++) {
        var solid = new bool[A];
        for (int a=0;a<A;a++) { int i = hor?a:b, j = hor?b:a;
          bool onLine = Get(E,hor,a,b) || Get(E,hor,a-1,b);
          bool cross = hor ? ((j>0&&i<NX&&V[i,j-1]) || (i<NX&&j<NY&&V[i,j])) : ((i>0&&j<NY&&H[i-1,j]) || (i<NX&&j<NY&&H[i,j]));
          solid[a] = onLine || cross; }
        int s=-1;
        for (int a=0;a<A;a++) {
          if (!solid[a]) continue;
          if (s>=0) { int gap=a-s; bool open=true; for (int k=s;k<a;k++) if (Get(E,hor,k,b)) open=false;
            bool inWall = Get(E,hor,s-1,b) || Get(E,hor,a,b);
            if (open && inWall && gap>=2 && gap<=maxGap) { int keep = s + gap/2; for (int k=s;k<a;k++) if (k!=keep) { Set(E,hor,k,b,true); changed++; } } }
          s=a;
        }
      } }
    return changed;
  }

  // A clean rectangular outer wall: frame pieces within Band px of it are dropped, and walls ending within Band of it
  // are extended to meet it.
  public static void Frame(int x0, int y0, int x1, int y1, int band) {
    int i0=x0/L, i1=x1/L, j0=y0/L, j1=y1/L, b=band/L;
    for (int i=0;i<NX;i++) for (int j=0;j<NY;j++) {
      if (H[i,j] && (Math.Abs(j-j0)<=b || Math.Abs(j-j1)<=b || j<j0 || j>j1)) H[i,j]=false;
      if (V[i,j] && (Math.Abs(i-i0)<=b || Math.Abs(i-i1)<=b || i<i0 || i>i1)) V[i,j]=false;
      if (H[i,j] && (i<i0 || i>=i1)) H[i,j]=false;
      if (V[i,j] && (j<j0 || j>=j1)) V[i,j]=false; }
    // Extend walls that stop short of the frame.
    for (int j=j0+1;j<j1;j++) {
      for (int k=1;k<=b+1;k++) if (H[i0+k,j]) { for (int i=i0;i<i0+k;i++) H[i,j]=true; break; }
      for (int k=1;k<=b+1;k++) if (H[i1-1-k,j]) { for (int i=i1-k;i<i1;i++) H[i,j]=true; break; } }
    for (int i=i0+1;i<i1;i++) {
      for (int k=1;k<=b+1;k++) if (V[i,j0+k]) { for (int j=j0;j<j0+k;j++) V[i,j]=true; break; }
      for (int k=1;k<=b+1;k++) if (V[i,j1-1-k]) { for (int j=j1-k;j<j1;j++) V[i,j]=true; break; } }
    for (int i=i0;i<i1;i++) { H[i,j0]=true; H[i,j1]=true; }
    for (int j=j0;j<j1;j++) { V[i0,j]=true; V[i1,j]=true; }
  }

  // Ink under a wall edge (share of its length with black pixels within 6 px).
  static double Coverage(bool hor, int i, int j) {
    int hit=0;
    for (int k=0;k<L;k++) { bool any=false;
      for (int o=-6;o<=6&&!any;o++) { int x = hor ? i*L+k : i*L+o, y = hor ? j*L+o : j*L+k; if (x>=0&&y>=0&&x<IW&&y<IH&&Ink[x,y]) any=true; }
      if (any) hit++; }
    return hit/(double)L;
  }
  // The sketch as drawn (5 px cells of ink, outside solid), for comparing connectivity.
  public static bool[,] InkGrid(int cell, int GW, int GH) {
    var g=new bool[GW,GH];
    for (int y=0;y<IH;y++) for (int x=0;x<IW;x++) if (Ink[x,y] && x/cell<GW && y/cell<GH) g[x/cell,y/cell]=true;
    var outside=new bool[GW,GH]; var q=new Queue<int>();
    for (int x=0;x<GW;x++){q.Enqueue(x); q.Enqueue((GH-1)*GW+x);} for (int y=0;y<GH;y++){q.Enqueue(y*GW); q.Enqueue(y*GW+GW-1);}
    while(q.Count>0){int k=q.Dequeue(); int x=k%GW,y=k/GW; if(outside[x,y]||g[x,y]) continue; outside[x,y]=true;
      if(x>0)q.Enqueue(k-1); if(x<GW-1)q.Enqueue(k+1); if(y>0)q.Enqueue(k-GW); if(y<GH-1)q.Enqueue(k+GW);}
    for (int y=0;y<GH;y++) for (int x=0;x<GW;x++) if (outside[x,y]) g[x,y]=true;
    return g;
  }
  // Reopens doorways: a wall edge separating two areas the sketch lets you walk between is removed (the least-inked
  // edge first, one per pair of areas). Edges inside the protected rectangles (x0,y0,x1,y1 flattened) stay.
  public static List<int[]> RestoreDoors(int[] protect) {
    int cell=5, GW=0, GH=0;
    var snapped = Grid(cell, 6, 1620, 1100, out GW, out GH);
    int ns; var sid = Walk.Regions(Walk.Walkable(snapped, 0), out ns);
    int no; var oid = Walk.Regions(Walk.Walkable(InkGrid(cell, GW, GH), 1), out no);
    var cands = new List<Tuple<double,bool,int,int,int,int>>();
    for (int i=0;i<NX;i++) for (int j=0;j<NY;j++) for (int d=0; d<2; d++) {
      bool hor = d==0; if (hor ? !H[i,j] : !V[i,j]) continue;
      int mx = hor ? i*L+L/2 : i*L, my = hor ? j*L : j*L+L/2;
      bool prot=false; for (int p=0;p+3<protect.Length;p+=4) if (mx>=protect[p]&&mx<=protect[p+2]&&my>=protect[p+1]&&my<=protect[p+3]) prot=true;
      if (prot) continue;
      // Each side: the first point (8-32 px out) that is floor in both the cleaned walls and the sketch.
      int sa=0, sb=0, oa=0, ob=0;
      for (int side=-1; side<=1; side+=2) for (int o=8; o<=32; o+=4) {
        int px = hor ? mx : mx+side*o, py = hor ? my+side*o : my;
        if (px<0||py<0||px>=GW*cell||py>=GH*cell) break;
        int s=sid[px/cell,py/cell], og=oid[px/cell,py/cell];
        if (s!=0 && og!=0) { if (side<0) { sa=s; oa=og; } else { sb=s; ob=og; } break; }
      }
      if (sa==0||sb==0||sa==sb||oa==0||oa!=ob) continue;
      cands.Add(Tuple.Create(Coverage(hor,i,j), hor, i, j, sa, sb));
    }
    cands.Sort((a,b)=>a.Item1.CompareTo(b.Item1));
    var parent=new int[ns+1]; for (int k=0;k<=ns;k++) parent[k]=k;
    Func<int,int> find=null; find = x => parent[x]==x ? x : (parent[x]=find(parent[x]));
    var opened=new List<int[]>();
    foreach (var c in cands) { int a=find(c.Item5), b=find(c.Item6); if (a==b) continue; parent[a]=b;
      if (c.Item2) H[c.Item3,c.Item4]=false; else V[c.Item3,c.Item4]=false;
      opened.Add(new[]{ c.Item2 ? c.Item3*L+L/2 : c.Item3*L, c.Item2 ? c.Item4*L : c.Item4*L+L/2, (int)(c.Item1*100) }); }
    return opened;
  }

  // Edits in sketch pixels: clear every edge inside a rectangle, or add a straight wall.
  public static void Clear(int x0, int y0, int x1, int y1) {
    for (int i=0;i<NX;i++) for (int j=0;j<NY;j++) {
      int hx=i*L+L/2, hy=j*L; if (hx>=x0&&hx<=x1&&hy>=y0&&hy<=y1) H[i,j]=false;
      int vx=i*L, vy=j*L+L/2; if (vx>=x0&&vx<=x1&&vy>=y0&&vy<=y1) V[i,j]=false; }
  }
  public static void Add(int x0, int y0, int x1, int y1) {
    int i0=(int)Math.Round(x0/(double)L), j0=(int)Math.Round(y0/(double)L), i1=(int)Math.Round(x1/(double)L), j1=(int)Math.Round(y1/(double)L);
    if (j0==j1) for (int i=Math.Min(i0,i1); i<Math.Max(i0,i1); i++) H[i,j0]=true;
    else for (int j=Math.Min(j0,j1); j<Math.Max(j0,j1); j++) V[i0,j]=true;
  }

  // Walls as a 5 px occupancy grid (wall thickness t px), for the walk checks.
  public static bool[,] Grid(int cell, int t, int w, int h, out int W, out int Hh) {
    int GW=(w+cell-1)/cell, GH=(h+cell-1)/cell; W=GW; Hh=GH; var g=new bool[GW,GH];
    Action<int,int,int,int> fill = (x0,y0,x1,y1) => { for (int y=Math.Max(0,y0/cell); y<=Math.Min(GH-1,y1/cell); y++) for (int x=Math.Max(0,x0/cell); x<=Math.Min(GW-1,x1/cell); x++) g[x,y]=true; };
    for (int i=0;i<NX;i++) for (int j=0;j<NY;j++) {
      if (H[i,j]) fill(i*L-t/2, j*L-t/2, (i+1)*L+t/2, j*L+t/2);
      if (V[i,j]) fill(i*L-t/2, j*L-t/2, i*L+t/2, (j+1)*L+t/2); }
    // Outside the walls is solid: flood from the border.
    var outside=new bool[W,Hh]; var q=new Queue<int>();
    for (int x=0;x<W;x++){q.Enqueue(x); q.Enqueue((Hh-1)*W+x);} for (int y=0;y<Hh;y++){q.Enqueue(y*W); q.Enqueue(y*W+W-1);}
    while(q.Count>0){int k=q.Dequeue(); int x=k%W,y=k/W; if(outside[x,y]||g[x,y]) continue; outside[x,y]=true;
      if(x>0)q.Enqueue(k-1); if(x<W-1)q.Enqueue(k+1); if(y>0)q.Enqueue(k-W); if(y<Hh-1)q.Enqueue(k+W);}
    for (int y=0;y<Hh;y++) for (int x=0;x<W;x++) if (outside[x,y]) g[x,y]=true;
    return g;
  }

  // Merged straight wall runs as x0,y0,x1,y1 (sketch pixels).
  public static List<int[]> Runs() {
    var list=new List<int[]>();
    for (int j=0;j<NY;j++) { int i=0; while(i<NX){ if(!H[i,j]){i++;continue;} int s=i; while(i<NX&&H[i,j]) i++; list.Add(new[]{s*L,j*L,i*L,j*L}); } }
    for (int i=0;i<NX;i++) { int j=0; while(j<NY){ if(!V[i,j]){j++;continue;} int s=j; while(j<NY&&V[i,j]) j++; list.Add(new[]{i*L,s*L,i*L,j*L}); } }
    return list;
  }
}
