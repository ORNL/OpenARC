package openacc.hir;

import java.io.*;
import java.lang.reflect.*;
import java.util.*;

import cetus.exec.Driver;
import cetus.hir.*;
import openacc.analysis.ACCParser;
import openacc.analysis.AnalysisTools;

/**
 * <b>TemplateFunctionCall</b> represents a C++ template function or method call.
 * 
 * @author Seyong Lee <lees2@ornl.gov>
 *         Future TechnologiesGroup, Oak Ridge National Laboratory
 *
 */
public class TemplateFunctionCall extends FunctionCall	
{
  private static Method class_print_method;
  private LinkedList<Printable> configuration;
  private Procedure linkedProcedure;

  static
  {
    Class[] params = new Class[2];

    try {
      params[0] = TemplateFunctionCall.class;
      params[1] = PrintWriter.class;
      class_print_method = params[0].getMethod("defaultPrint", params);
    } catch (NoSuchMethodException e) {
      throw new InternalError();
    }
  }

  /**
   * Creates a function call.
   *
   * @param function An expression that evaluates to a function.
   */
  public TemplateFunctionCall(Expression function)
  {
    super(function);
    configuration = new LinkedList<Printable>();
    object_print_method = class_print_method;
    
  }

  /**
   * Creates a function call.
   *
   * @param function An expression that evaluates to a function.
   * @param args A list of arguments to the function.
   */
  public TemplateFunctionCall(Expression function, List args)
  {
    super(function, args);
    configuration = new LinkedList<Printable>();
    object_print_method = class_print_method;
  }

/**
   * Creates a function call.
   *
   * @param function An expression that evaluates to a function.
   * @param args A list of arguments to the function.
   * @param confargs A list of configuration arguments to the function.
   */
  public TemplateFunctionCall(Expression function, List args, List confargs)
  {
    super(function, args);
    configuration = new LinkedList<Printable>();
    object_print_method = class_print_method;
    setConfArguments(confargs);
  }

  /**
   * Prints a function call to a stream.
   *
   * @param call The call to print.
   * @param p printwriter
   */
  public static void defaultPrint(TemplateFunctionCall call, PrintWriter p)
  {
	  List conflist = call.getConfArguments();

	  if (call.needs_parens)
		  p.print("(");


      call.getName().print(p);
	  p.print("<");
      PrintTools.printListWithComma(conflist, p);
      p.print(">");
      p.print("(");
      PrintTools.printListWithComma(call.children.subList(1, call.children.size()), p); 
      p.print(")");

     if (call.needs_parens)
         p.print(")");
  }

	public String toString()
	{
		StringBuilder str = new StringBuilder(80);

		if ( needs_parens )
			str.append("(");

		str.append(getName());
		str.append("<");
		List tmp = configuration;
		str.append(PrintTools.listToString(tmp, ", "));
		str.append(">");
		str.append("(");
		tmp = (new ChainedList()).addAllLinks(children);
		tmp.remove(0);
		str.append(PrintTools.listToString(tmp, ", "));
		str.append(")");

		if ( needs_parens )
			str.append(")");

		return str.toString();
	}

  public Printable getConfArgument(int n)
  {
    return (Printable)configuration.get(n);
  }

  public List getConfArguments()
  {
    return configuration;
  }

  public void setConfArgument(int n, Printable templ)
  {
	if( n >= configuration.size() ) {
		configuration.add(templ);
	} else {
		configuration.set(n, templ);
	}
  }

  public void addConfArgument(Printable templ)
  {
	configuration.add(templ);
  }

  public void setConfArguments(List args)
  {
    configuration.clear();
    //configuration.addAll(args);
	for(Object o : args)
	{
      Printable templ = null;
      try {
        templ = (Printable)o;
      } catch (ClassCastException e) {
        throw new IllegalArgumentException();
      }
      configuration.add(templ);
	}
  }

/**
   * Overrides the class print method, so that all subsequently
   * created objects will use the supplied method.
   *
   * @param m The new print method.
   */
  static public void setClassPrintMethod(Method m)
  {
    class_print_method = m;
  }

   public void setLinkedProcedure(Procedure proc)
   {
       linkedProcedure = proc;
   }
   
   @Override
   public void accept(TraversableVisitor v) {
     ((OpenACCTraversableVisitor)v).visit(this);
   }
}
