package ssca2;
import core.GlobalJobRunner;
import x10.compiler.Pragma;
import x10.compiler.Inline;
import x10.util.Random;
import core.LifelineGenerator;
import x10.compiler.Ifdef;
import x10.compiler.Uncounted;

import x10.util.Option;
import x10.util.OptionsParser;
import core.TaskBag;
import core.LocalJobRunner;
public class MySSCA2 extends GlobalJobRunner[SSCA2TaskItem, SSCA2Result]{
	
	private var resultReducer:Reducible[SSCA2Result];
	public def getResultReducer():x10.lang.Reducible[SSCA2Result] {
		// TODO: auto-generated method stub
		return this.resultReducer;
	}
	
	public def setResultReducer(var r:x10.lang.Reducible[SSCA2Result]):void {
		this.resultReducer = r;
	}
	
// 	public static def parseCMDLine(args:Rail[String]):SSCA2TaskFrame{
// 		val cmdLineParams = new OptionsParser(args, new Rail[Option](0L), [
// 		                                                                   Option("s", "", "Seed for the random number"),
// 		                                                                   Option("n", "", "Number of vertices = 2^n"),
// 		                                                                   Option("a", "", "Probability a"),
// 		                                                                   Option("b", "", "Probability b"),
// 		                                                                   Option("c", "", "Probability c"),
// 		                                                                   Option("d", "", "Probability d"),
// 		                                                                   Option("p", "", "Permutation"),
// 		                                                                   Option("v", "", "Verbose")]);
// 
// 		val seed:Long = cmdLineParams("-s", 2);
// 		val n:Int = cmdLineParams("-n", 2n);
// 		val a:Double = cmdLineParams("-a", 0.55);
// 		val b:Double = cmdLineParams("-b", 0.1);
// 		val c:Double = cmdLineParams("-c", 0.1);
// 		val d:Double = cmdLineParams("-d", 0.25);
// 		val permute:Int = cmdLineParams("-p", 1n); // on by default
// 		val verbose:Int = cmdLineParams("-v", 0n); // off by default
// 
// 		Console.OUT.println("Running SSCA2 with the following parameters:");
// 		Console.OUT.println("seed = " + seed);
// 		Console.OUT.println("N = " + (1<<n));
// 		Console.OUT.println("a = " + a);
// 		Console.OUT.println("b = " + b);
// 		Console.OUT.println("c = " + c);
// 		Console.OUT.println("d = " + d);
// 		Console.OUT.println("places = " + Place.numPlaces());
// 
// 		return  new SSCA2TaskFrame(Rmat(seed, n, a, b, c, d),(1<<n/2)as Int, permute, verbose);
// 		
// 	}
	
	public static def main(args:Rail[String]) {
		val opts = new OptionsParser(args, new Rail[Option](), [
		                                                      
		                                                        Option("s", "", "seed to generate random graph"),
		                                                        Option("n", "", "Number of vertices = 2^n"),
		                                                        Option("a", "", "a in SSCA2"),
		                                                        Option("b", "", "b in SSCA2"),
		                                                        Option("c", "", "c in SSCA2"),
		                                                        Option("d", "", "d in SSCA2"),
		                                                        Option("p", "", "permute or not"),
		                                                        Option("v", "", "Verbose. Default 0 (no)"),
		                                                       	Option("i", "", "Interval length"),
		                                                       	Option("t", "", "split threshold"),
		                                                       	Option("g", "", "granularity, i.e., N in the atMostN"),
		                                                        Option("w", "", "Number of thieves to send out. Default 1."),
		                                                        Option("l", "", "Base of the lifeline"),
		                                                        Option("m", "", "Max potential victims")]);
		val seed:Long = opts("-s", 2);
		val n:Int = opts("-n", 2n);
		val a:Double = opts("-a", 0.55);
		val b:Double = opts("-b", 0.1);
		val c:Double = opts("-c", 0.1);
		val d:Double = opts("-d", 0.25);
		val permute:Int = opts("-p", 1n); // on by default
		val verbose:Int = opts("-v", 0n); // off by default

		val i:Int = opts("-i", 4n); // change to 4 to get a better performance
		val t:Int = opts("-t", 1n);
		val g = opts("-g", 1n);
		
		val l = opts("-l", 2n); // default should be 2, so workload can distribute fast
		val m = opts("-m", 1024n);
	

		val P = Place.numPlaces();

		var z0:Int = 1n;
		var zz:Int = l; 
		while (zz < P) {
			z0++;
			zz *= l;
		}
		val z = z0; 

		val w = opts("-w", z);

		Console.OUT.println("Running SSCA2-G with the following parameters:");
		Console.OUT.println("seed = " + seed);
		Console.OUT.println("N = " + (1<<n));
		Console.OUT.println("a = " + a);
		Console.OUT.println("b = " + b);
		Console.OUT.println("c = " + c);
		Console.OUT.println("d = " + d);
		Console.OUT.println("places = " + Place.numPlaces());
		Console.OUT.println("interval = " + i);
		Console.OUT.println("l = " + l);
		Console.OUT.println("permute = " + permute);
		val myssca2: MySSCA2 = new MySSCA2();
		// uncomment it to get it work
		myssca2.setResultReducer(new SSCA2ResultReducible((1<<n) as Int));
		var time:Long = System.nanoTime();
		//result:Long = myuts.main(()=>new LocalJobRunner[UTSTreeNode, Long](new UTSTaskFrame(b,r,d), n, w, l, z, m));
		result:SSCA2Result = myssca2.main(
				()=>new LocalJobRunner[SSCA2TaskItem, SSCA2Result](new SSCA2TaskFrame(Rmat(seed, n, a, b, c, d),i,t, permute, verbose),
				g, w, l, z, m));
		time = System.nanoTime() - time;
		//Console.OUT.println("Time spent: " + (time/1E9));
		
		//printBetweennessMap(result.betweennessMap, (1<<n) as Int);
		// end of uncomment it to get it work
		
		//myuts.dryRun(()=>new LocalJobRunner[UTSTreeNode, Long](new UTSTaskFrame(b,r,d), n, w, l, z, m));
	
		
	}
	
	protected static def printBetweennessMap(betweennessMap:Rail[Double], N:Int) {
		for(var i:Int=0n; i<N; ++i) {
			if(betweennessMap(i) != 0.0) {
				Console.OUT.println("(" + i + ") -> " + sub(""+betweennessMap(i), 0n, 6n));
			}
		}
	}

	private static def sub(str:String, start:Int, end:Int) = str.substring(start, Math.min(end, str.length()));
	
	
	
}
