/* McCaughan's maximally marvellous moby maze making machine
 * (c) 1995 Gareth McCaughan
 *
 * Usage: make-maze <x> <y> [<seed>]
 *
 * Make mazes using Olin Shivers's method (actually he didn't invent it):
 * start with our set of cells; randomly knock down walls unless
 * doing so introduces a cycle into the connectivity graph of our
 * cells.
 * Like Shivers, I'll do hexagonal mazes: it's more fun that way.
 *
 * If you are compiling under SunOS 4, put -DSUN on the command line.
 * If you're compiling on some other platform, and any changes are
 * needed to the program to make it compile correctly, please let
 * me know and I'll create a Makefile and maybe even a configure script.
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifdef SUN
# include <memory.h>
# include <sys/timeb.h>
# define Random random
# define SRandom srandom
# define CLOCKS_PER_SEC 1000000
# define difftime(x,y) (((double)x)-(double)y)
#else
# include <string.h>
# define Random rand	/* Beware bad random number generators! */
# define SRandom srand
#endif

/* The cells are represented by an array pointed to by |cells|.
 * A negative entry -N means "This cell is in a component of size N".
 * A non-negative entry M means "This cell is in a component whose
 * description you can find by looking in cell number M, which is
 * another thing in the same component".
 * If two cells are in the same component, then repeated chaining
 * will lead you to the same final cell. Otherwise it won't. We
 * avoid having to do too much chaining because every time we
 * do any we go back and snap all the pointers so that they point
 * straight at the final cell, IYSWIM.  This trick is due to
 * Tarjan.
 */
static int *cells;

/* If |x| is a cell, then |base(x)| is the cell one reaches by
 * chaining along those pointers from |x|.
 */
static int base(int x) {
  int y=cells[x],z=x;
  while (y>=0) { z=y; y=cells[y]; }
  /* Now we could just return |z|, but we snap pointers first. */
  y=x; while (cells[y]>=0) { x=cells[y]; cells[y]=z; y=x; }
  return z;
}

/* If |x| and |y| are base cells (i.e., |cells[x]<0| and |cells[y]<0|)
 * then |unify(x,y)| puts |x| and |y| in the same component.
 */
static void unify(int x, int y) {
  int sx=cells[x],sy=cells[y];
  if (sx<sy) { /* X larger than Y */
    cells[y]=x;
    cells[x]=sx+sy;
  }
  else {
    cells[x]=y;
    cells[y]=sx+sy;
  }
}

/* After making the maze, we need to convert it into a slightly
 * more helpful format. To do this, we need another array of
 * information about cells. Here's the information we need
 * about each cell:
 */
typedef struct node {
  int exits;	/* bitmap representing exits -- see later */
  int n_kids;	/* number of descendants in tree */
  struct node *kids[6];	/* and which ones they are */
  /* The use of the following will become clear when you look
   * at |analyse_tree|.
   */
  struct node *first,*second,*furthest;
  int length,distance;
} node;

/* And here's the array.
 */
node *nodes;

/* Exits from a cell are recorded in a bitmap. It only needs 8 bits,
 * even allowing a little spare space. Thus:
 */
enum {
  Up=1, Down=2,		/* (k,l) -> (k,l+-1)    delta=+-1 */
  LDown=4, RDown=8,	/* (k,l) -> (k+-1,l-1)  delta=+-n_rows -1 */
  LEq=16, REq=32,	/* (k,l) -> (k+-1,l)    delta=+-n_rows    */
  LUp=64, RUp=128	/* (k,l) -> (k+-1,l+1)  delta=+-n_rows +1 */
};

/* Set up the |cells| array to contain |n| cells, each in its own
 * component. Also set up the |nodes| array to contain |n| nodes,
 * none of them with any children or any walls.
 */
static void init_cells(int n) {
  cells=malloc(n*sizeof(int));
  if (!cells) {
    fprintf(stderr,"! I couldn't get enough memory for |cells|.\n");
    exit(1);
  }
  memset(cells,-1,n*sizeof(int));	/* strictly, this isn't portable... */
  nodes=calloc(n,sizeof(node));
  if (!nodes) {
    fprintf(stderr,"! I couldn't get enough memory for |nodes|.\n");
    exit(1);
  }
}

/* We need to go through the walls between the cells in a random order.
 * To do this, we produce an array of structures representing walls,
 * and sort it into random order.
 */
typedef struct wall {
  struct wall *next;	/* next wall after this one */
  int lower;	/* one neighbouring cell */
  int higher;	/* the other neighbouring cell */
} wall;

/* The array of walls is pointed to by |walls|. Amazing.
 */
static wall *walls;

/* And the first wall to be dealt with is pointed to by |first_wall|.
 * (The last one is indicated by having a null pointer in its |next|
 * field, of course.)
 */
static wall *first_wall;

/* More than one bit of the program needs to know how big the
 * maze is. Here are the dimensions:
 */
static int n_columns;
static int n_rows;

/* Set up the |walls| array to represent the walls in an |m| by |n|
 * hexagonal array of cells, and link them all together with their
 * |next| fields.
 * Now is a good time to think about how to describe our cells.
 * I adopt Shivers's terminology: columns are numbered left to right,
 * 0..m-1, and each column contains n cells 0..n-1. Columns 0,2,...
 * are half a cell "lower" than columns 1,3,... .
 * Thus, cell (k,l) is adjacent to cells (k,l+-1) above and below;
 * and to cells (k+-1,l) and (k+-1,l-1) if k is even,
 * or to cells  (k+-1,l) and (k+-1,l+1) if k is odd.
 * Except for the walls, of course: if any of those numbers are
 * outside their ranges then the relevant cells just aren't there.
 * The total number of walls is (m-1)*(2n-1) horizontally, plus
 * m*(n-1) vertically: that comes to 2mn-m-2n+1+mn-m = 3mn-2m-2n+1.
 * Cell (i,j) is number |n*i+j| in the |cells| array: so we go
 * up the columns first, as it were.
 * This is not efficient code, by the way; but it's not radically
 * inefficient, and I don't think this is where the program spends
 * most of its time.
 */
static void init_walls(int m, int n) {
  int i,j;
  int this;
  wall *e;
  walls=malloc((3*m*n-m-m-n-n+1)*sizeof(wall));
  if (!walls) {
    fprintf(stderr,"! I couldn't get enough memory for |walls|.\n");
    exit(1);
  }
  e=walls;this=0;
  for (i=0;i<m;++i) {
    for (j=0;j<n;++j) {
      /* Cell (i,j). */
      if (i>0 && (j<n-1 || !(i&1))) {	/* Northwest: */
        e->next=e+1;
        e->lower=this;
        e->higher=this-n+(i&1);
        ++e;
      }
      if (j<n-1) {	/* North: */
        e->next=e+1;
        e->lower=this;
        e->higher=this+1;
        ++e;
      }
      if (i<m-1 && (j<n-1 || !(i&1))) {	/* Northeast: */
        e->next=e+1;
        e->lower=this;
        e->higher=this+n+(i&1);
        ++e;
      }
      ++this;
    }
  }
  if (e!=walls+3*m*n-m-m-n-n+1) {
    fprintf(stderr,"! Gareth screwed up (%d != %d).\n",e-walls,3*m*n-m-m-n-n+1);
    exit(1);
  }
  (e-1)->next=0;	/* terminate list */
}

/* If this isn't going to give the exact same maze every time we run it,
 * we'd better start up the random number generator in some way that
 * actually depends on a few things.
 * |init_rand()| does this: it should be called before |init_walls|.
 * If |seed| is non-zero, we use that directly as our seed.
 */
static int seed;
static void init_rand(void) {
#ifdef SUN
  struct timeb t;
  int pid;
  if (!seed) {
    ftime(&t);
    pid=getpid();
    seed=(t.time<<8) + (t.millitm) + (pid<<16);
  }
#else
  seed=clock()+time(0);
#endif
  seed=seed&0x7FFFFFFF;
  SRandom(seed);
}

/* To sort the walls into order, we do three passes of 1024-way radix sort,
 * putting things into random bins. This is equivalent to giving
 * each wall a random 30-bit key and sorting into order, but doesn't
 * require us to waste space storing keys we don't really want.
 * So, here are the list heads for the sublists:
 */
static wall *head1[1024];	/* initialised to 0, say ANSI */
static wall *head2[1024];	/* ditto */

/* "Sort" the walls into random order.
 */
static void shuffle_walls(void) {
  wall *p,*q;
  int i,k;
  /* First pass, into head1[]. */
  p=walls; while (p) {
    k=Random()&1023;
    q=p->next;
    p->next=head1[k]; head1[k]=p;
    p=q;
  }
  /* Second pass, from head1[] into head2[]. */
  for (i=0;i<1024;++i) {
    p=head1[i]; head1[i]=0; while (p) {
      k=Random()&1023;
      q=p->next;
      p->next=head2[k]; head2[k]=p;
      p=q;
    }
  }
  /* Third pass, from head2[] into head1[]. */
  for (i=0;i<1024;++i) {
    p=head2[i]; head2[i]=0; while (p) {
      k=Random()&1023;
      q=p->next;
      p->next=head1[k]; head1[k]=p;
      p=q;
    }
  }
  /* Finally, link them all together again. */
  q=0;
  for (i=0;i<1024;++i) {
    p=head1[i];
    if (!p) continue;
    if (q) { q->next=p; } else first_wall=p;
    while (p) { q=p; p=p->next; }
  }
  /* We're done. */
}

/* Create the maze: for each wall, see whether its neighbours are
 * already in the same component. If so, do nothing (removing this
 * wall would mean that there were two paths between some pair of
 * points). If not, remove the wall (by unlinking it from the list)
 * and unify the components of the cells on either side.
 * When we remove a wall, we also put an entry in the |exits| field
 * of each corresponding |node|.
 */
static void create_maze(void) {
  wall *p=first_wall;
  wall *q=0;	/* if |q!=0| then |q->next| is waiting to be filled in */
  int x,y;
  int delta;
  while (p) {
    x=base(p->lower); y=base(p->higher);
    if (x!=y) {
      unify(x,y);	/* not connected: connect them */
      /* Also, add exits. */
#define Add_exits(dh,dl) { \
  nodes[p->higher].exits|=dh; nodes[p->lower].exits|=dl; }
      delta=p->higher-p->lower;
      if (delta==1) Add_exits(Down,Up)
      else if (delta>0) switch(delta-n_rows) {
        case -1: Add_exits(LUp,RDown); break;
        case 0: Add_exits(LEq,REq); break;
        case 1: Add_exits(LDown,RUp); }
      else /* delta<0 */ switch(delta+n_rows) {
        case -1: Add_exits(RUp,LDown); break;
        case 0: Add_exits(REq,LEq); break;
        case 1: Add_exits(RDown,LUp);
      }
    }
    else { if (q) q->next=p; else first_wall=p; q=p; }/* connected: refrain from deleting wall */
    p=p->next;
  }
  q->next=0;	/* terminate list */
}

/* Build a subtree, starting at node |n| and including as much of the
 * full tree as is possible without including anything already visited.
 * Return 0 if this node has in fact already been visited; otherwise 1.
 * Calling this for the first time builds an entire tree, with root at
 * node |n|.
 */
static int build_tree(node *n) {
  int exits=n->exits;
  int nk=0;
  if (exits&0x80000000) return 0;	/* already visited? */
  n->exits|=0x80000000;			/* mark as visited */
  /* Visit all neighbours: */
  if (exits&LDown) if (build_tree(n-n_rows-1)) n->kids[nk++]=n-n_rows-1;
  if (exits&LEq)   if (build_tree(n-n_rows))   n->kids[nk++]=n-n_rows;
  if (exits&LUp)   if (build_tree(n-n_rows+1)) n->kids[nk++]=n-n_rows+1;
  if (exits&Down)  if (build_tree(n-1))        n->kids[nk++]=n-1;
  if (exits&Up)    if (build_tree(n+1))        n->kids[nk++]=n+1;
  if (exits&RDown) if (build_tree(n+n_rows-1)) n->kids[nk++]=n+n_rows-1;
  if (exits&REq)   if (build_tree(n+n_rows))   n->kids[nk++]=n+n_rows;
  if (exits&RUp)   if (build_tree(n+n_rows+1)) n->kids[nk++]=n+n_rows+1;
  n->n_kids=nk;
  return 1;
}

/* Fill in some fields of the |node|s of the subtree at |n|.
 * |first|,|second| are the ends of the longest path in this
 * subtree; |length| is its length. |furthest| is the most
 * distant (from |n|) single node, and |distance| is its
 * distance from |n|.
 * The points are that (1) we can compute all this stuff
 * recursively, and (2) when we've done so for the entire
 * tree, we know what the two maximally-distant nodes are.
 * This algorithm was suggested to me by Colin Bell, but
 * it's obvious enough that it's probably been thought of
 * before.
 * We actually use a strange metric: paths with lots of branches
 * count for more than paths with few branches. This probably
 * makes for a harder maze.
 */
void analyse_tree(node *n) {
  int i=n->n_kids;
  node *m;
  int d1=0,d2=0;	/* biggest & next-biggest distance to leaves */
  int l1=0;		/* longest path among subtrees */
  node *dn1=0,*dn2=0;	/* nodes achieving d1,d2 */
  node *ln1=0;		/* node achieving l1 */
  /* First off, deal with leaf nodes */
  if (!i) {
    n->first=n->second=n->furthest=n;
    n->length=n->distance=0;
    return;
  }
  /* The general case: */
  while (--i>=0) {
    m=n->kids[i];
    analyse_tree(m);
    if (m->length>=l1) { l1=m->length; ln1=m; }
    if (m->distance>=d1) { d2=d1; dn2=dn1; d1=m->distance; dn1=m; }
    else if (m->distance>=d2) { d2=m->distance; dn2=m; }
  }
  d1+=n->n_kids; d2+=n->n_kids;
  n->distance=d1; n->furthest=dn1->furthest;
  if (d1+d2>l1) {
    n->length=d1+d2;
    n->first=dn1->furthest;
    n->second=dn2 ? dn2->furthest : n;	/* this deals with case n_kids==1 */
  }
  else {
    n->length=l1;
    n->first=ln1->first;
    n->second=ln1->second;
  }
}

/* Print out the maze.
 * |start| and |end| are the starting and ending points of the maze,
 * of course; they're integers using the same correspondence as
 * everywhere else in the program.
 * We produce PostScript; we define three operators:
 * N draws the north wall of a given cell, NE,NW draw the NE,NW walls.
 * Cells are given as <place in column> <column number>.
 */
#define Check { if ((nn+=10)>=70) { printf("\n"); nn=0; } else printf(" "); }
#define Check1 { if ((nn+=2)>=70) { printf("\n"); nn=0; } else printf(" "); }
static void print_maze(int start, int end) {
  wall *p=first_wall;
  double xs=500/((n_columns+1)*1.36602540378444);
  double ys=700/((n_rows+1)*1.73205080756888);
  double scale = (xs<ys) ? xs : ys;
  int i,j,x;
  int nn=0;	/* number of walls on current line */
  printf("%%!PS\n");
  printf("/Times-Roman findfont 10 scalefont setfont\n");
  printf("30 770 moveto (Maze produced by ) show\n");
  printf("/Times-Italic findfont 10 scalefont setfont\n");
  printf("(make-maze ) show\n");
  printf("/Times-Roman findfont 10 scalefont setfont\n");
  printf("30 755 moveto (Parameters: %dx%d, seed=%d) show\n",
         n_columns,n_rows,seed);
  printf("\n30 40 translate\n");
  printf("%lg %lg scale\n",scale,scale);
  printf("1 1 translate\n");
  printf("\n");
  printf("/M { dup 1 and 0 ne { exch .5 add exch } if\n");
  printf("     1.5 mul exch\n");
  printf("     1.73205080756888 mul\n");
  printf("     newpath moveto } bind def\n");
  printf("/N { gsave -.6 0.866025403784439 rmoveto\n");
  printf("     .15 .0866025403784439 rlineto\n");
  printf("     .9 0 rlineto\n");
  printf("     .15 -.0866025403784439 rlineto\n");
  printf("     -.15 -.0866025403784439 rlineto\n");
  printf("     -.9 0 rlineto\n");
  printf("     closepath fill grestore } bind def\n");
  printf("/NW{ gsave -.45 .952627944162883 rmoveto\n");
  printf("     0 -.173205080756888 rlineto\n");
  printf("     -.45 -.779422863405995 rlineto\n");
  printf("     -.15 -.0866025403784439 rlineto\n");
  printf("     0 .173205080756888 rlineto\n");
  printf("     .45 .779422863405995 rlineto\n");
  printf("     closepath fill grestore } bind def\n");
  printf("/NE{ gsave .45 .952627944162883 rmoveto\n");
  printf("     0 -.173205080756888 rlineto\n");
  printf("     .45 -.779422863405995 rlineto\n");
  printf("     .15 -.0866025403784439 rlineto\n");
  printf("     0 .173205080756888 rlineto\n");
  printf("     -.45 .779422863405995 rlineto\n");
  printf("     closepath fill grestore } bind def\n");
  printf("/A { 0 1.73205080756888 rmoveto\n");
  printf("     currentpoint newpath moveto } bind def\n");
  printf("/B { N A } bind def\n");
  printf("/C { NW A } bind def\n");
  printf("/D { NW N A } bind def\n");
  printf("/E { NE A } bind def\n");
  printf("/F { N NE A } bind def\n");
  printf("/G { NW NE A } bind def\n");
  printf("/H { NW N NE A } bind def\n");
  /* Outer walls: */
  printf("\n%% Outer walls:\n");
  printf("-1 -1 M NE"); Check;
  for (i=1;i<n_rows;++i) { printf("A NE"); Check; }
  printf("0 0 M NW"); Check;
  for (i=1;i<n_rows;++i) { printf("A NW"); Check; }
  printf("0 %d M NE",n_columns-1); Check;
  for (i=1;i<n_rows;++i) { printf("A NE"); Check; }
  printf("%d %d M NW",-(n_columns&1),n_columns); Check;
  for (i=1;i<n_rows;++i) { printf("A NW"); Check; }
  for (i=0;i<n_columns;++i) {
    printf("-1 %d M N",i); Check;
    printf("%d %d M N",n_rows-1,i); Check;
    if (i&1) {
      printf("-1 %d M NW",i); Check;
      if (i<n_columns-1) { printf("-1 %d M NE",i); Check; }
      printf("%d %d M NW",n_rows-1,i); Check;
      printf("%d %d M NE",n_rows-1,i); Check;
    }
  }
  if (nn) printf("\n");
  /* Inner walls: */
  printf("\n%% Inner walls:\n");
  for (i=0;i<n_columns;++i) {
    printf("0 %d M",i); Check;
    for (j=0;j<n_rows;++j) {
      x=nodes[i*n_rows+j].exits;
      if (i&1) x = ((x&Up) ? 0 : 1) | ((x&LUp) ? 0 : 2) | ((x&RUp) ? 0 : 4);
          else x = ((x&Up) ? 0 : 1) | ((x&LEq) ? 0 : 2) | ((x&REq) ? 0 : 4);
      printf("%c",65+x); Check1;
    }
  }
#if 0
  while (p) {
    printf("%d %d ",p->lower%n_rows,p->lower/n_rows);
    if (p->higher-p->lower==1) printf("N");
    else if (p->higher>p->lower) printf("NE");
    else printf("NW");
    Check;
    p=p->next;
  }
#endif
  if (nn) printf("\n");
  /* Start and end points: */
  printf("\n%% Start and end of path:\n");
  printf("%d %d M currentpoint 0.3 0 360 arc fill\n",start%n_rows,start/n_rows);
  printf("%d %d M currentpoint 0.3 0 360 arc fill\n",end%n_rows,end/n_rows);
  printf("\nshowpage\n");
}

/* It's nice to have some idea of how long all this is taking.
 * So we keep track of the elapsed time and CPU time.
 */
static double cpu0,cpu1;	/* initial & most recent */
static time_t real0,real1;	/* initial & most recent */

/* Display the CPU time and real time so far.
 */
static void show_time(void) {
  clock_t cpu=clock();
  time_t real=time(0);
  if (cpu!=(clock_t)-1)
    fprintf(stderr,"cpu=%5.03lf (total %5.03lf)  ",
            ((double)cpu-cpu1)/CLOCKS_PER_SEC,
            ((double)cpu-cpu0)/CLOCKS_PER_SEC);
  if (real!=(time_t)-1)
    fprintf(stderr,"real=%lg (total %lg)",
            difftime(real,real1),
            difftime(real,real0));
  fprintf(stderr,"\n");
  cpu1=cpu; real1=real;
}

/* Before we use |show_time| we need to set up those variables.
 */
static void init_time(void) {
  cpu0=cpu1=(double)clock();
  real0=real1=time(0);
}

/* Now everything's trivial!
 */
int main(int argc, char *argv[]) {
  if (argc!=3 && argc!=4) {
    fprintf(stderr,"Usage: %s <columns> <rows> [<seed>]\n",argv[0]);
    return 0;
  }
  n_columns=atoi(argv[1]);
  n_rows=atoi(argv[2]);
  if (n_columns<2 || n_rows<2 || n_columns>1000 || n_rows>1000) {
    fprintf(stderr,"Both dimensions must be in the range 2..1000.\n");
    return 1;
  }
  if (argc==4) seed=atoi(argv[3]);

  fprintf(stderr,"Initialising everything... ");
  init_time();
  init_rand();
  init_cells(n_rows*n_columns);
  init_walls(n_columns,n_rows);
  show_time();

  fprintf(stderr,"Shuffling walls...         ");
  shuffle_walls();
  show_time();

  fprintf(stderr,"Creating maze...           ");
  create_maze();
  show_time();

  fprintf(stderr,"Building tree...           ");
  build_tree(nodes);
  show_time();

  fprintf(stderr,"Analysing tree...          ");
  analyse_tree(nodes);
  show_time();

  fprintf(stderr,"Printing maze...           ");
  print_maze(nodes->first-nodes,nodes->second-nodes);
  show_time();

  fprintf(stderr,"Done.\n");
  return 0;
}
