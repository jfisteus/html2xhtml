/*
 * DTDDeclHandler.java
 *
 * (Jesús Arias Fisteus)
 *
 * 'Handler' que recibe las notificaciones de decleraciones
 * encontradas en los DTD.
 *
 * Una vez finalizado el análisis de los DTD, genera los
 * ficheros dtd.c y dtd.h con esta información codificada
 * para ser usada en el resto del programa.
 *
 *
 * $Id: DTDDeclHandler.java,v 1.5 2005/01/29 19:38:44 jaf Exp $
 *
 */


import org.xml.sax.ext.DeclHandler;
import org.xml.sax.SAXException;

import java.util.Vector;
import java.util.StringTokenizer;
import java.util.Date;
import java.text.SimpleDateFormat;
import java.io.PrintStream;
import java.io.FileOutputStream;



/**
 * StringTokenizer con varios métodos auxiliares para
 * recuperar el último token emitido
 *
 */
class MyStringTokenizer extends StringTokenizer {

    private static String ultimo= null;

    /**
     * @see StringTokenizer#StringTokenizer(String,String,boolean)
     *
     */
    public MyStringTokenizer(String str, String delim, boolean tipo) {
        super(str,delim,tipo);
    }

    public String nextToken() {
        String devolver;

        if (ultimo!= null) {
            devolver= ultimo;
            ultimo= null;
        } else devolver= super.nextToken();

        return devolver;
    }

    public boolean hasMoreTokens() {
        if (ultimo!= null) return true;
        else return super.hasMoreTokens();
    }

    public void recuperar(String v) {
        ultimo= v;
    }

}



/**
 * Esta clase guarda los datos asociados a la declaración de un
 * atributo
 *
 */
class DeclAtt {

    /* tipos de atributo */
    public static final int ATTTYPE_CDATA =       -2;
    public static final int ATTTYPE_ID =          -3;
    public static final int ATTTYPE_IDREF =       -4;
    public static final int ATTTYPE_IDREFS =      -5;
    public static final int ATTTYPE_ENTITY =      -6;
    public static final int ATTTYPE_ENTITIES =    -7;
    public static final int ATTTYPE_NMTOKEN =     -8;
    public static final int ATTTYPE_NMTOKENS =    -9;
    public static final int ATTTYPE_ENUMERATED = -10;
    public static final int ATTTYPE_NOTATION =   -11;

    /* tipos de valores por defecto */
    public static final int DEFDECL_DEFAULT =      0;
    public static final int DEFDECL_REQUIRED =     1;
    public static final int DEFDECL_IMPLIED =      2;
    public static final int DEFDECL_FIXED =        3;

    public String   nombre;
    public int      tipo;
    public int      obligatorio;
    public int      defecto= -1;
    public int      dtd;              /* máscara de DTD */
    public int      id;
    public String   tipoString= null;
    public String   defectoString;

    /**
     * inserta un nuevo atributo en la lista de atributos, y
     * devuelve su identificador.
     * si ya existe, devuelve el identificador del existente
     *
     */
    public DeclAtt(String nombre, String tipo, String obligatorio,
                   String defectoString, int dtd) throws Exception {

        this.nombre= nombre;

        /* establece el tipo de atributo */
        if (tipo.compareTo("CDATA")==0)
            this.tipo= ATTTYPE_CDATA;
        else if (tipo.compareTo("ID")==0)
            this.tipo= ATTTYPE_ID;
        else if (tipo.compareTo("IDREF")==0)
            this.tipo= ATTTYPE_IDREF;
        else if (tipo.compareTo("IDREFS")==0)
            this.tipo= ATTTYPE_IDREFS;
        else if (tipo.compareTo("ENTITY")==0)
            this.tipo= ATTTYPE_ENTITY;
        else if (tipo.compareTo("ENTITIES")==0)
            this.tipo= ATTTYPE_ENTITIES;
        else if (tipo.compareTo("NMTOKEN")==0)
            this.tipo= ATTTYPE_NMTOKEN;
        else if (tipo.compareTo("NMTOKENS")==0)
            this.tipo= ATTTYPE_NMTOKENS;
        else if (tipo.compareTo("NOTATION")==0)
            throw new Exception("tipo de atributo NOTATION no soportado");
        else {
            this.tipo= ATTTYPE_ENUMERATED;
            this.tipoString= tipo;
        }

        /* establece el tipo de valor por defecto */
        if (obligatorio==null)
            this.obligatorio= DEFDECL_DEFAULT;
        else if (obligatorio.compareTo("#REQUIRED")==0)
            this.obligatorio= DEFDECL_REQUIRED;
        else if (obligatorio.compareTo("#IMPLIED")==0)
            this.obligatorio= DEFDECL_IMPLIED;
        else if (obligatorio.compareTo("#FIXED")==0)
            this.obligatorio= DEFDECL_FIXED;
        else
            throw new Exception("tipo de valor por defecto no soportado: "
                                + tipo);

        this.dtd= (1<<dtd);
        if ((this.obligatorio== DEFDECL_DEFAULT)
            || (this.obligatorio== DEFDECL_FIXED)) {
            this.defectoString= defectoString;
        }
    }



    /**
     * Busca en el vector 'lista' un atributo que coincida
     * con el especificado.
     * Devuelve dicho elemento, o null si no se encuentra
     *
     */
    public static DeclAtt buscaAtt(Vector lista, DeclAtt clave) {

        for (int i=0; i<lista.size(); i++)
            if (((DeclAtt)lista.elementAt(i)).nombre.compareTo(clave.nombre)==0) {
                DeclAtt att= (DeclAtt)lista.elementAt(i);

                if ((clave.tipo==att.tipo)
                    && (clave.obligatorio==att.obligatorio)
                    && (((clave.defectoString==null)&&(att.defectoString==null))
                        ||(clave.defectoString.compareTo(att.defectoString)==0))
                    && (((clave.tipoString==null)&&(att.tipoString==null)) ||
                        (clave.tipoString.compareTo(att.tipoString)==0)))
                    return att;
            }

        /* no se encuentra */
        return null;
    }


    /**
     * Busca en el vector 'lista' un atributo cuyo nomre se 'nombre'
     *
     * Devuelve un vector con todos los atributos coincidentes
     *
     */
    public static Vector buscaAtt(Vector lista, String nombre) {
        Vector atts= new Vector();

        for (int i=0; i<lista.size(); i++)
            if (((DeclAtt)lista.elementAt(i)).nombre.
                compareTo(nombre)==0) {

                atts.addElement(lista.elementAt(i));
            }

        return atts;
    }
}





/**
 * esta clase guarda los datos asociados a la declaración de un elemento
 *
 */
class DeclElm {

    public static final int CONTENIDO_NADA= 0;
    public static final int CONTENIDO_EMPTY= 1;
    public static final int CONTENIDO_ANY= 2;
    public static final int CONTENIDO_MIXED= 3;
    public static final int CONTENIDO_CHILDREN= 4;


    /**
     * valores de constantes para codificación de contenido de elementos
     *
     */
    private static final int CSPEC_PAR_O=     1;
    private static final int CSPEC_PAR_C=     2;
    private static final int CSPEC_CHOICE=    0x10;
    private static final int CSPEC_AST=       0x04;
    private static final int CSPEC_MAS=       0x08;
    private static final int CSPEC_INT=       0x0C;


    public String   nombre;
    public int[]    tipoContenido;
    public String[] contenido;
    public Vector[] listaAtt;
    public int[]    ptrContenido;
    public int      dtd;
    public int      id;

    static private DeclElm ultimo= null;
    static public  String elmBuffer= "";

    public DeclElm(String nombre, int numDtds) {
        this.nombre= nombre;
        dtd= 0;

        tipoContenido= new int[numDtds];
        for (int i=0; i<numDtds; i++)
            tipoContenido[i]= 0;


        contenido= new String[numDtds];
        for (int i=0; i<numDtds; i++)
            contenido[i]= null;

        ptrContenido= new int[numDtds];
        for (int i=0; i<numDtds; i++)
            ptrContenido[i]= -1;

        listaAtt= new Vector[numDtds];
        for (int i=0; i<numDtds; i++)
            listaAtt[i]= new Vector();
    }


    public void estableceDatos(int dtd, String contenido)
        throws SAXException {

        this.dtd= this.dtd | (1<<dtd);

        /* averigua el tipo de contenido */
        try {
            if (contenido.substring(0,3).compareTo("ANY")==0) {
                tipoContenido[dtd]= CONTENIDO_ANY;
                this.contenido[dtd]= null;
                return;
            }
        } catch (StringIndexOutOfBoundsException ex) {
            // nothing
        }

        try {
            if (contenido.substring(0,5).compareTo("EMPTY")==0) {
                tipoContenido[dtd]= CONTENIDO_EMPTY;
                this.contenido[dtd]= null;
                return;
            }
        } catch (StringIndexOutOfBoundsException ex) {

        }



        /* ¿será child o mixed?   NOTA: llega con los espacios filtrados */
        if (contenido.charAt(0)!='(')
            throw new SAXException("Contenido incorrecto");


        try {
            if (contenido.substring(1,8).compareTo("#PCDATA")==0)
                tipoContenido[dtd]= CONTENIDO_MIXED;
            else tipoContenido[dtd]= CONTENIDO_CHILDREN;
        } catch (StringIndexOutOfBoundsException ex) {
            tipoContenido[dtd]= CONTENIDO_CHILDREN;
        }

        this.contenido[dtd]= contenido;
    }


    /**
     * Busca en el vector 'lista' un elemento cuyo nombre coincida
     * con el especificado.
     * Devuelve dicho elemento, o null si no se encuentra
     *
     */
    public static DeclElm buscaElm(String nombre, Vector lista) {

        if ((ultimo!=null) && (ultimo.nombre.compareTo(nombre)==0))
            return ultimo;

        for (int i=0; i<lista.size(); i++)
            if (((DeclElm)lista.elementAt(i)).nombre.compareTo(nombre)==0) {
                ultimo= (DeclElm)lista.elementAt(i);
                return ultimo;
            }

        /* no se encuentra */
        return null;
    }


    /**
     * Codifica el contenido de un elemento y lo inserta en el buffer
     *
     * Devuelve el puntero a la posición del buffer
     *
     */
    public static int codificaContenido(Vector elementos,
                                        int tipo, String contenido) {
        String codificado= codificaContenido(elementos,contenido,tipo);
        int pos;

        if ((pos=elmBuffer.indexOf(codificado))==-1) {
            pos= elmBuffer.length();
            elmBuffer= elmBuffer.concat(codificado);
        }

        return pos;
    }



    private static String codificaContenido(Vector elementos, String contenido,
                                            int tipo) {
        char[] cars= new char[1024];
        int    ptr= 0;

        if (tipo== CONTENIDO_MIXED) {
            // empieza con (#PCDATA: se leen
            MyStringTokenizer tokenizer=
                new MyStringTokenizer(contenido,"()|*",false);

            if (tokenizer.nextToken().compareTo("#PCDATA")!=0)
                System.err.println("WARNING: error en contenido MIXED");

            while (tokenizer.hasMoreTokens()) {
                String nombreElm= tokenizer.nextToken();
                DeclElm elm= DeclElm.buscaElm(nombreElm,elementos);
                if (elm==null)
                    System.err.println("WARNING: "+nombreElm
                                       +", elemento no encontrado");
                else {
                    cars[ptr++]= (char)(128+elm.id);
                }
            }

        } else {
            // tipo children: se llama a una subfunción
            MyStringTokenizer tokenizer=
                new MyStringTokenizer(contenido,"()|,",true);

            try {
                ptr= codificaContenidoChild(tokenizer,cars,ptr,elementos);
            } catch (Exception ex){
                System.err.println("ERROR (codificaContenidoChild): "
                                   +ex.getMessage());
                ex.printStackTrace();
                System.exit(1);
            }
        }

        // finaliza con un 0
        cars[ptr++]= 0;

        return new String(cars,0,ptr);
    }





    /**
     * función recursiva para codificar los contenidos de tipo children
     *
     */
    private static int codificaContenidoChild(MyStringTokenizer tok,
                                              char[] buffer, int ptr,
                                              Vector elementos)
        throws Exception {

        boolean cerrar;
        int posIni= ptr;
        String v;

        // primero se recibe '('
        if (tok.nextToken().compareTo("(")!=0)
            throw new Exception("se espera (");
        buffer[ptr++]= CSPEC_PAR_O;

        // bucle leyendo el contenido
        while (true) {
            v= tok.nextToken();

            if (v.compareTo(")")==0) break; //fin de recursión
            else if (v.compareTo("(")==0) {//nueva recursión
                tok.recuperar(v);
                ptr= codificaContenidoChild(tok,buffer,ptr,elementos);
            }

            else if (v.compareTo(",")==0); //nuevo elemento de la secuencia, no hago nada

            else if (v.compareTo("|")==0) {
                //nuevo elemento del choice
                //se marca y se sigue leyendo
                buffer[posIni] |= CSPEC_CHOICE;
            }

            else { //elemento: se codifica
                switch (v.charAt(v.length()-1)) {
                case '*':
                    buffer[ptr++]= CSPEC_PAR_O | CSPEC_AST;
                    v= v.substring(0,v.length()-1);
                    cerrar= true;
                    break;
                case '+':
                    buffer[ptr++]= CSPEC_PAR_O | CSPEC_MAS;
                    v= v.substring(0,v.length()-1);
                    cerrar= true;
                    break;
                case '?':
                    buffer[ptr++]= CSPEC_PAR_O | CSPEC_INT;
                    v= v.substring(0,v.length()-1);
                    cerrar= true;
                    break;
                default:
                    cerrar= false;
                }

                DeclElm elm= DeclElm.buscaElm(v,elementos);
                if (elm==null)
                    throw new Exception(v+": no se encuentra elemento");

                buffer[ptr++]= (char)(128+elm.id);
                if (cerrar)
                    buffer[ptr++]= CSPEC_PAR_C;
            }

        } //while(1)

        // se codifica el paréntesis de cierre
        buffer[ptr++]= CSPEC_PAR_C;

        if (tok.hasMoreTokens()) {
            v= tok.nextToken();
            if (v.compareTo("*")==0) buffer[posIni]|= CSPEC_AST;
            else if (v.compareTo("+")==0) buffer[posIni]|= CSPEC_MAS;
            else if (v.compareTo("?")==0) buffer[posIni]|= CSPEC_INT;
            else tok.recuperar(v);
        }

        return ptr;
    }
}





/**
 *  Clase que implementa el Handler para declaraciones de XML
 *  como, por ejemplo, las del DTD
 *
 */
class DTDDeclHandler implements DeclHandler {

    private Vector entities= new Vector();
    private Vector elementos= new Vector();
    private Vector atributos= new Vector();
    private int    dtd= 0;
    private int    numDtds;

    private int    entMaxLon;
    private int    attMaxLon;
    private int    elmMaxLon;
    private int    elmMaxNumAtt;

    private String attBuffer= null;


    // lista de elementos clave, para generar su macro
    //   de identificador en dtd.h
    private String[] listaElmClave= new String [] {
        "html","head","body","frameset","style","script","meta","p",
        "title","pre","frame","applet","a","form","iframe","img","map",
        "ul","ol","li","table","tr","th","td","thead","tbody","object",
        "big","small","sub","sup","input","select","textarea","label",
        "button", "fieldset", "isindex", "center", "u", "s", "strike",
        "ins", "del", "area"};
    private String[] listaAttClave= new String [] {
      "xml:space","http-equiv","content","xmlns"};


    public DTDDeclHandler(int numDtds) {

        this.numDtds= numDtds;
    }


    public void elementDecl(String name, String model)
        throws SAXException
    {
//      System.err.println("elementDecl: ");
//      System.err.println("   "+name);
//      System.err.println("   "+model);

        DeclElm elm= DeclElm.buscaElm(name, elementos);

        if (elm==null) {
            elm= new DeclElm(name, numDtds);
            elm.id= elementos.size();
            elementos.add(elm);
        }

        elm.estableceDatos(dtd, model);
    }


    public void attributeDecl(String eName, String aName,
                              String type, String valueDefault,
                              String value)
        throws SAXException
    {

//      System.err.println("attributeDecl: ");
//      System.err.println("   "+eName);
//      System.err.println("   "+aName);
//      System.err.println("   "+type);
//      System.err.println("   "+valueDefault);
//      System.err.println("   "+value);

        /* crea y enlaza el atributo */
        DeclAtt clave;

        try {
            clave= new DeclAtt(aName,type,valueDefault,value,dtd);
        } catch (Exception ex) {
            throw new SAXException("attributeDecl : "+ex.getMessage());
        }

        DeclAtt att= DeclAtt.buscaAtt(atributos,clave);
        if (att==null) {
            clave.id= atributos.size();
            atributos.addElement(clave);
            att= clave;
        } else {
            att.dtd= att.dtd | (1<<dtd);
        }

        /* asocia el atributo al elemento */
        DeclElm elm= DeclElm.buscaElm(eName, elementos);
        if (elm==null) {
            /* A veces aparece antes el ATTLIST que el ELEMENT (xhtml 1.1) */
            elm= new DeclElm(eName, numDtds);
            elm.id= elementos.size();
            elementos.add(elm);
        }

        elm.listaAtt[dtd].addElement(new Integer(att.id));
    }


    public void internalEntityDecl(String name, String value)
        throws SAXException
    {

//      System.err.println("internalEntityDecl: ");
//      System.err.println("   "+name+": "+value+" ["+"]");

        /* en principio, sólo se almacena el nombre de la entidad */
        if (name.charAt(0)!='%')
          if (entities.indexOf(name)==-1)
            entities.addElement(name);
    }


    public void externalEntityDecl(String name, String publicId,
                                   String systemId)
        throws SAXException
    {
//        System.err.println("externalEntityDecl: ");
//        System.err.println("   "+name);
//        System.err.println("   "+publicId);
//        System.err.println("   "+systemId);
    }

    /** establece el número de dtd para las siguientes declaraciones */
    protected void establecerDtd(int num) {
        dtd= num;
    }

    /** una vez finalizado, se generan los ficheros .c y .h */
    protected void finalizar(String outputPath) {
        finalizarAtributos(atributos);
        finalizarElementos(elementos);

        try {
            // imprime el fichero dtd.c
            FileOutputStream outStr= new FileOutputStream(outputPath+"/dtd.c");
            PrintStream out= new PrintStream(outStr);
            System.out.println("Escribiendo " + outputPath + "/dtd.c");
            imprimirCabecera(out);
            imprimirEntidades(out);
            imprimirAtributos(out);
            imprimirElementos(out);
            out.close();


            // imprime el fichero dtd.h
            outStr= new FileOutputStream(outputPath+"/dtd.h");
            out= new PrintStream(outStr);
            System.out.println("Escribiendo " + outputPath + "/dtd.h");
            imprimirDtdH(out);
            out.close();
        } catch (java.io.FileNotFoundException ex) {
            System.err.println(ex.getMessage());
        }
    }



    /**
     * crea el buffer de valores de los atributos
     *
     */
    private void finalizarAtributos(Vector atributos) {
        attBuffer= "";
        int maxLon= 0;

        for (int i=0; i<atributos.size(); i++) {
            DeclAtt att= (DeclAtt)atributos.elementAt(i);

            if (att.nombre.length()>maxLon) maxLon= att.nombre.length();

            if (att.tipo== DeclAtt.ATTTYPE_ENUMERATED) {
                // inserta el tipo enumerado en el buffer
                att.tipo= attBuffer.length();
                attBuffer= attBuffer + att.tipoString + '\0';
            }

            if ((att.obligatorio== DeclAtt.DEFDECL_DEFAULT)
                || (att.obligatorio== DeclAtt.DEFDECL_FIXED)) {
                // inserta el valor fijo o por defecto en el buffer
                att.defecto= attBuffer.length();
                attBuffer= attBuffer + att.defectoString + '\0';
            }
        }

        attMaxLon= maxLon+1;
    }



    /**
     * crea el buffer de contenidos de elementos
     *
     */
    private void finalizarElementos(Vector elementos) {
        int numElm= elementos.size();
        int numMaxAtt=0;
        int maxLon= 0;

        for (int i=0; i<numElm; i++) {
            DeclElm elm=(DeclElm)elementos.elementAt(i);
            for (int nDtd=0; nDtd< numDtds; nDtd++) {
                if ((elm.tipoContenido[nDtd]==DeclElm.CONTENIDO_MIXED)
                    ||(elm.tipoContenido[nDtd]==DeclElm.CONTENIDO_CHILDREN)) {


                    //lo codifica
                    elm.ptrContenido[nDtd]=
                        DeclElm.codificaContenido(elementos,
                                                  elm.tipoContenido[nDtd],
                                                  elm.contenido[nDtd]);
                }

                // número de atributos
                if (elm.listaAtt[nDtd].size() > numMaxAtt)
                    numMaxAtt= elm.listaAtt[nDtd].size();

            } // for nDtd

            // longitud del nombre
            if (elm.nombre.length() > maxLon) maxLon= elm.nombre.length();

        } // for i

        elmMaxLon= maxLon + 1;
        elmMaxNumAtt= numMaxAtt + 1;
    }



    /**
     * imprime la cabecera del fichero c:
     *   - comentario
     *   - includes
     *
     */
    private void imprimirCabecera(PrintStream out) {
        out.println("/*");
        out.println(" * fichero generado mediante DTDCoder");
        out.println(" * NO EDITAR");
        out.println(" *");
        out.println(" * codificación de los datos de los DTD");
        out.println(" *");
        out.println(" */");
        out.println("");
        out.println("#include \"dtd.h\"");
        out.println("");
    }






    private void imprimirEntidades(PrintStream out) {

        /* busca el nombre más largo para fijar el tamaño del array */
        int max= 0;
        for (int i=0; i<entities.size(); i++)
            if (((String)entities.elementAt(i)).length()>max)
                max=((String)entities.elementAt(i)).length();

        //out.println("int ent_data_num= "+entities.size()+";");

        out.println("char ent_list["+entities.size()+"]["+(max+1)+"]= {");

        /* calcula el valor de hash de cada entidad */
        int[] valorHash= new int[entities.size()];

        for (int i=0; i<entities.size(); i++) {
            int hash= ((String)entities.elementAt(i)).hashCode() %256;

            if (hash>=0) valorHash[i]= hash;
            else valorHash[i]= hash+256;
        }

        boolean coma= false;

        int[] tablaHash= new int[256];

        for (int k=0, pos=0; k<256; k++) {
            tablaHash[k]= pos;
            for (int i=0; i<entities.size(); i++) {
                if (valorHash[i]==k) {
                    if (coma) out.println(",");
                    else out.println("");
                    coma= true;
                    pos++;
                    out.print ("    \""+entities.elementAt(i)+"\"  /*"+
                               valorHash[i]+'*'+"/");
                }
            }
        }
        out.println("");
        out.println("};");

        // imprime la tabla hash asociada
        out.println("/* tabla hash para entidades */");
        out.print("int ent_hash[257]= {");

        for (int i=0; i < 256; i++) {
            if ((i & 0xf)==0) out.println("");
            String ns= String.valueOf(tablaHash[i]);
            while (ns.length()<3) ns=" "+ns;

            if (i==0) out.print(" "+ns);
            else out.print(","+ns);
        }
        out.println("\n,"+entities.size()+"};\n");


        entMaxLon= max + 1;
    }


    private void imprimirElementos(PrintStream out) {
        out.println("\n/* elementos */");
        //out.println("int elm_data_num= "+elementos.size()+";");
        out.println("elm_data_t elm_list["+elementos.size()+"]= {");

        boolean primero= true;

        for (int i=0; i<elementos.size(); i++) {
            DeclElm elm= (DeclElm) elementos.elementAt(i);

            if (!primero) out.println(",");
            else primero= false;

            out.print("    {\""+elm.nombre+"\",{"+elm.tipoContenido[0]);
            for (int k= 1; k< numDtds; k++)
                out.print(","+elm.tipoContenido[k]);

            out.print("},{"+elm.ptrContenido[0]);
            for (int k= 1; k< numDtds; k++)
                out.print(","+elm.ptrContenido[k]);

            out.println("},");

            out.print("        {\n");


            // imprime las listas de atributos
            for (int k=0; k<numDtds; k++) {
                int l;
                out.print("           {");
                for (l=0; l<elm.listaAtt[k].size(); l++) {
                    out.print(elm.listaAtt[k].elementAt(l)+",");
                    if ((l==15)||(l==31)) out.print("\n            ");
                }
                for ( ; l<(elmMaxNumAtt-1); l++) {
                    out.print("-1,");
                    if ((l==15)||(l==31)) out.print("\n            ");

                }
                out.print("-1}");
                if (k<numDtds-1) out.println(",");
                else out.print("\n       },");
            }
            out.println(" "+elm.dtd+"}");

        } // for i

        out.println("};\n");

        // buffer de contenidos
        out.println("/* buffer de datos para contenido de elementos */");
        //out.println("int elm_buffer_num= "+DeclElm.elmBuffer.length()+";");
        out.print("\nunsigned char elm_buffer["+DeclElm.elmBuffer.length()+
                  "]= {");

        char[] caracteres= DeclElm.elmBuffer.toCharArray();

        for (int i=0; i < caracteres.length; i++) {
            if ((i & 0xf)==0) out.println("");
            int num= (int)caracteres[i];
            String ns= String.valueOf(num);
            while (ns.length()<3) ns=" "+ns;

            if (i==0) out.print(" "+ns);
            else out.print(","+ns);
        }
        out.println("\n};");

    }


    private void imprimirAtributos(PrintStream out) {
        out.println("\n/* atributos */");
        //out.println("int att_data_num= "+atributos.size()+";");
        out.println("att_data_t att_list["+atributos.size()+"]= {");

        int num= atributos.size();
        for (int i=0; i< num; i++) {
            DeclAtt att= (DeclAtt) atributos.elementAt(i);

            out.print("    {\""+att.nombre+"\","+ att.tipo + ","
                      + att.obligatorio +
                      "," + att.defecto + ","+att.dtd+"}");

            if ((i+1)== num) out.println("");
            else out.println(",");
        } // for i

        out.println("};\n");

        out.println("/* buffer de datos para los atributos */");
//         out.println("\nchar att_buffer["+attBuffer.length()
//                     +"]= \""+attBuffer+"\";");

        out.print("\nunsigned char att_buffer["+attBuffer.length()+
                  "]= {");

        char[] caracteres= attBuffer.toCharArray();

        for (int i=0; i < caracteres.length; i++) {
            if ((i & 0xf)==0) out.println("");
            num= (int)caracteres[i];
            String ns= String.valueOf(num);
            while (ns.length()<3) ns=" "+ns;

            if (i==0) out.print(" "+ns);
            else out.print(","+ns);
        }
        out.println("\n};");

    }



    private void imprimirDtdH(PrintStream out) {
        SimpleDateFormat sdf = new SimpleDateFormat("yyyy-MM-dd HH:mm:ssZ");
        String date = sdf.format(new Date()).toString();

        out.println("/*");
        out.println(" * fichero generado mediante DTDCoder");
        out.println(" * NO EDITAR");
        out.println(" *");
        out.println(" * codificación de los datos de los DTD");
        out.println(" * fichero de declaraciones");
        out.println(" *");
        out.println(" */");
        out.println("");

        out.println("#ifndef DTD_H\n#define DTD_H\n");
        out.println("#define DTD_NUM "+numDtds+"\n");
        out.println("#define ATT_NAME_LEN "+attMaxLon);
        out.println("#define ELM_NAME_LEN "+elmMaxLon);
        out.println("#define ENT_NAME_LEN "+entMaxLon+"\n");
        out.println("#define ELM_ATTLIST_LEN "+elmMaxNumAtt+"\n");

        out.println("#define DTD_SNAPSHOT_DATE \""+date+"\"\n");

        out.println("/* ficheros declarando tipos y otras macros */");
        out.println("#include \"dtd_types.h\"\n");
        out.println("#include \"dtd_names.h\"\n");

        out.println("#define elm_data_num "+elementos.size());
        out.println("#define att_data_num "+atributos.size());
        out.println("#define ent_data_num "+entities.size());
        out.println("#define elm_buffer_num "+ DeclElm.elmBuffer.length());
        out.println("#define att_buffer_num "+ attBuffer.length());

        out.println("\n/* variables declaradas en dtd.c */");
        out.println("extern elm_data_t elm_list[];");
        out.println("extern unsigned char elm_buffer[];");
        out.println("extern att_data_t att_list[];");
        out.println("extern unsigned char att_buffer[];\n");

        out.println("extern int ent_hash[257];");
        out.println("extern char ent_list[]["+entMaxLon+"];\n");


        /* id de elementos y atributos clave */
        for (int i=0; i<listaElmClave.length; i++)
            elmClave(listaElmClave[i],out);
        for (int i=0; i<listaAttClave.length; i++)
            attClave(listaAttClave[i],out);

        out.println("#endif /* DTD_H */");
    }


    private void elmClave(String nombre, PrintStream out) {
        DeclElm elm= DeclElm.buscaElm(nombre,elementos);

        if (elm!=null)
            out.println("#define ELMID_"+nombre.toUpperCase()+" "+elm.id);
    }

    private void attClave(String nombre, PrintStream out) {

        Vector atts= DeclAtt.buscaAtt(atributos,nombre);
        if (atts.size()==1) {
            DeclAtt att= (DeclAtt) atts.elementAt(0);
            out.println("#define ATTID_"+nombre.toUpperCase().replace(':','_')
                        .replace('-','_') +" "+ att.id);
        } else if (atts.size() > 1) {
            DeclAtt att;
            for (int i=0; i<atts.size(); i++) {
                att= (DeclAtt) atts.elementAt(i);
                out.println("#define ATTID_"+nombre.toUpperCase().replace(':','_')
                            .replace('-','_')+"_"+i +" "+ att.id);
            }
        }
    }
}




