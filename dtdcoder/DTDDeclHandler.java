/***************************************************************************
 *   Copyright (C) 2001-2008 by Jesus Arias Fisteus                        *
 *   jaf@it.uc3m.es                                                        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

/*
 * DTDDeclHandler.java
 *
 * Receives events from the DTD parser and stores data locally. At the end
 * generates the C source files with the gathered data.
 *
 */

import java.io.FileOutputStream;
import java.io.PrintWriter;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.StringTokenizer;
import java.util.Vector;

import org.xml.sax.SAXException;
import org.xml.sax.ext.DeclHandler;


/**
 * StringTokenizer with some auxiliarly methods
 * for processing again the last token.
 *
 */
class MyStringTokenizer extends StringTokenizer {

    private static String last= null;

    /**
     * @see StringTokenizer#StringTokenizer(String,String,boolean)
     *
     */
    public MyStringTokenizer(String str, String delim, boolean type) {
        super(str, delim, type);
    }

    public String nextToken() {
        String token;

        if (last!= null) {
            token = last;
            last = null;
        } else token = super.nextToken();

        return token;
    }

    public boolean hasMoreTokens() {
        if (last != null) return true;
        else return super.hasMoreTokens();
    }

    public void getLast(String v) {
        last = v;
    }

}

/**
 * Internal class representing an attribute
 *
 */
class AttDecl {

    /* attribute types */
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

    /* default-value types */
    public static final int DEFDECL_DEFAULT =      0;
    public static final int DEFDECL_REQUIRED =     1;
    public static final int DEFDECL_IMPLIED =      2;
    public static final int DEFDECL_FIXED =        3;

    public String   name;
    public int      type;
    public int      required;
    public int      defaultId = -1;
    public int      dtd;              /* DTD mask */
    public int      id;
    public String   stringType = null;
    public String   defaultString;

    /**
     * Insert a new attribute in the attribute list and return
     * its identifier. If the attribute already exists, just returns
     * its identifier.
     *
     */
    public AttDecl(String name, String type, String required,
                   String defaultString, int dtd) throws Exception {

        this.name= name;

        /* set the attribute type */
        if (type.compareTo("CDATA")==0)
            this.type= ATTTYPE_CDATA;
        else if (type.compareTo("ID")==0)
            this.type= ATTTYPE_ID;
        else if (type.compareTo("IDREF")==0)
            this.type= ATTTYPE_IDREF;
        else if (type.compareTo("IDREFS")==0)
            this.type= ATTTYPE_IDREFS;
        else if (type.compareTo("ENTITY")==0)
            this.type= ATTTYPE_ENTITY;
        else if (type.compareTo("ENTITIES")==0)
            this.type= ATTTYPE_ENTITIES;
        else if (type.compareTo("NMTOKEN")==0)
            this.type= ATTTYPE_NMTOKEN;
        else if (type.compareTo("NMTOKENS")==0)
            this.type= ATTTYPE_NMTOKENS;
        else if (type.compareTo("NOTATION")==0)
            throw new Exception("tipo de atributo NOTATION no soportado");
        else {
            this.type= ATTTYPE_ENUMERATED;
            this.stringType= type;
        }

        /* set attribute default-value type */
        if (required==null)
            this.required= DEFDECL_DEFAULT;
        else if (required.compareTo("#REQUIRED")==0)
            this.required= DEFDECL_REQUIRED;
        else if (required.compareTo("#IMPLIED")==0)
            this.required= DEFDECL_IMPLIED;
        else if (required.compareTo("#FIXED")==0)
            this.required= DEFDECL_FIXED;
        else
            throw new Exception("unsupported default-value type: "
                                + type);

        this.dtd= (1<<dtd);
        if ((this.required== DEFDECL_DEFAULT)
            || (this.required== DEFDECL_FIXED)) {
            this.defaultString= defaultString;
        }
    }



    /**
     * Look for an attribute in the given vector.
     *
     */
    public static AttDecl searchAtt(Vector<AttDecl> list, AttDecl key) {

        for (int i = 0; i < list.size(); i++) {
            if (list.get(i).name.compareTo(key.name) == 0) {
                AttDecl att = list.get(i);

                if ((key.type == att.type)
                    && (key.required == att.required)
                    && (((key.defaultString == null) && (att.defaultString == null))
                        ||(key.defaultString.compareTo(att.defaultString) == 0))
                    && (((key.stringType == null)&&(att.stringType == null)) ||
                        (key.stringType.compareTo(att.stringType) == 0)))
                    return att;
            }
        }

        /* not found */
        return null;
    }


    /**
     * Look for attributes with the given name. Returns all the
     * attributes with that name.
     *
     */
    public static Vector<AttDecl> searchAttByName(Vector<AttDecl> list, String name) {
        Vector<AttDecl> atts = new Vector<AttDecl>();

        for (int i = 0; i < list.size(); i++)
            if (list.elementAt(i).name.compareTo(name) == 0) {
                atts.add(list.elementAt(i));
            }

        return atts;
    }
}


/**
 * esta clase guarda los datos asociados a la declaración de un elemento
 *
 */
class ElmDecl {

    public static final int CONTENT_NOTHING = 0;
    public static final int CONTENT_EMPTY = 1;
    public static final int CONTENT_ANY = 2;
    public static final int CONTENT_MIXED = 3;
    public static final int CONTENT_CHILDREN = 4;


    /**
     * valores de constantes para codificación de contenido de elementos
     *
     */
    private static final int CSPEC_PAR_O =    1;
    private static final int CSPEC_PAR_C =    2;
    private static final int CSPEC_CHOICE =   0x10;
    private static final int CSPEC_AST =      0x04;
    private static final int CSPEC_MAS =      0x08;
    private static final int CSPEC_INT =      0x0C;


    public String name;
    public int[] contentType;
    public String[] content;
    public Vector<Integer>[] attList;
    public int[] contentPtr;
    public int dtd;
    public int id;

    static private ElmDecl last = null;
    static public  String elmBuffer = "";

    public ElmDecl(String name, int numDtds) {
        this.name = name;
        dtd = 0;

        contentType = new int[numDtds];
        for (int i = 0; i < numDtds; i++)
            contentType[i]= 0;

        content = new String[numDtds];
        for (int i = 0; i < numDtds; i++)
            content[i]= null;

        contentPtr = new int[numDtds];
        for (int i = 0; i < numDtds; i++)
            contentPtr[i]= -1;

        attList = new Vector[numDtds];
        for (int i = 0; i < numDtds; i++)
            attList[i] = new Vector<Integer>();
    }

    public void setData(int dtd, String content)
        throws SAXException {

        this.dtd= this.dtd | (1 << dtd);

        /* decide content type */
        try {
            if (content.substring(0,3).compareTo("ANY") == 0) {
                contentType[dtd] = CONTENT_ANY;
                this.content[dtd] = null;
                return;
            }
        } catch (StringIndexOutOfBoundsException ex) {
            // nothing
        }

        try {
            if (content.substring(0,5).compareTo("EMPTY") == 0) {
                contentType[dtd] = CONTENT_EMPTY;
                this.content[dtd] = null;
                return;
            }
        } catch (StringIndexOutOfBoundsException ex) {

        }

        // ¿children or mixed?   NOTE: spaces are already filtered
        if (content.charAt(0) != '(')
            throw new SAXException("Incorrect content");

        try {
            if (content.substring(1, 8).compareTo("#PCDATA") == 0)
                contentType[dtd] = CONTENT_MIXED;
            else contentType[dtd] = CONTENT_CHILDREN;
        } catch (StringIndexOutOfBoundsException ex) {
            contentType[dtd] = CONTENT_CHILDREN;
        }

        this.content[dtd] = content;
    }


    /**
     * Look in the vector for an element with a given name
     * Returns the element, or null if not found
     *
     */
    public static ElmDecl searchElm(String name, Vector<ElmDecl> list) {

        if ((last != null) && (last.name.compareTo(name) == 0))
            return last;

        for (int i = 0; i < list.size(); i++) {
            if (list.elementAt(i).name.compareTo(name) == 0) {
                last = list.elementAt(i);
                return last;
            }
        }

        /* not found */
        return null;
    }


    /**
     * Encode the element's content and store it in the buffer
     *
     * Returns a pointer to its position in the buffer
     *
     */
    public static int encodeContent(Vector<ElmDecl> elements, int type, String content) {
        String encoded = encodeContentInternal(elements, type, content);
        int pos;

        if ((pos = elmBuffer.indexOf(encoded)) == -1) {
            pos = elmBuffer.length();
            elmBuffer = elmBuffer.concat(encoded);
        }

        return pos;
    }

    private static String encodeContentInternal(Vector<ElmDecl> elements, int type,
                                                String content) {
        char[] chars = new char[1024];
        int    ptr = 0;

        if (type == CONTENT_MIXED) {
            // begins with "(#PCDATA"?: reed it
            MyStringTokenizer tokenizer=
                new MyStringTokenizer(content, "()|*", false);

            if (tokenizer.nextToken().compareTo("#PCDATA") != 0)
                System.err.println("WARNING: error in MIXED content");

            while (tokenizer.hasMoreTokens()) {
                String nombreElm= tokenizer.nextToken();
                ElmDecl elm = ElmDecl.searchElm(nombreElm,elements);
                if (elm == null)
                    System.err.println("WARNING: " + nombreElm
                                       +", element not found");
                else {
                    chars[ptr++] = (char)(128 + elm.id);
                }
            }
        } else {
            // type children: call another function to encode it
            MyStringTokenizer tokenizer=
                new MyStringTokenizer(content, "()|,", true);

            try {
                ptr = encodeChildrenContent(tokenizer, chars, ptr, elements);
            } catch (Exception ex){
                System.err.println("ERROR (encodeChildrenContent): "
                                   + ex.getMessage());
                ex.printStackTrace();
                System.exit(1);
            }
        }

        // finish it with a 0
        chars[ptr++] = 0;

        return new String(chars, 0, ptr);
    }

    /**
     * recursive function that encodes children content rules
     *
     */
    private static int encodeChildrenContent(MyStringTokenizer tok,
                                              char[] buffer, int ptr,
                                              Vector<ElmDecl> elements)
        throws Exception {

        boolean close;
        int posIni = ptr;
        String v;

        // '(' first
        if (tok.nextToken().compareTo("(") != 0)
            throw new Exception("ERROR: '(' expected");
        buffer[ptr++] = CSPEC_PAR_O;

        // reads content
        while (true) {
            v = tok.nextToken();

            if (v.compareTo(")") == 0 ) {
                break; // end recursion
            } else if (v.compareTo("(") == 0) {// new recursive call
                tok.getLast(v);
                ptr = encodeChildrenContent(tok, buffer, ptr, elements);
            } else if (v.compareTo(",")==0) {
                //new element: do nothing
            } else if (v.compareTo("|") == 0) {
                // new element in choice
                // mark and continue reading
                buffer[posIni] |= CSPEC_CHOICE;
            } else { //element: encode it
                switch (v.charAt(v.length() - 1)) {
                case '*':
                    buffer[ptr++] = CSPEC_PAR_O | CSPEC_AST;
                    v = v.substring(0, v.length() - 1);
                    close = true;
                    break;
                case '+':
                    buffer[ptr++] = CSPEC_PAR_O | CSPEC_MAS;
                    v = v.substring(0, v.length() - 1);
                    close = true;
                    break;
                case '?':
                    buffer[ptr++] = CSPEC_PAR_O | CSPEC_INT;
                    v = v.substring(0, v.length() - 1);
                    close = true;
                    break;
                default:
                    close = false;
                }

                ElmDecl elm = ElmDecl.searchElm(v, elements);
                if (elm == null)
                    throw new Exception(v + ": element not found");

                buffer[ptr++]= (char)(128 + elm.id);
                if (close)
                    buffer[ptr++] = CSPEC_PAR_C;
            }
        } //while(1)

        // encode closing
        buffer[ptr++] = CSPEC_PAR_C;

        if (tok.hasMoreTokens()) {
            v = tok.nextToken();
            if (v.compareTo("*") == 0) buffer[posIni] |= CSPEC_AST;
            else if (v.compareTo("+") == 0) buffer[posIni] |= CSPEC_MAS;
            else if (v.compareTo("?") == 0) buffer[posIni] |= CSPEC_INT;
            else tok.getLast(v);
        }

        return ptr;
    }
}


/**
 *  Handler for DTD declarations
 *
 */
class DTDDeclHandler implements DeclHandler {

    private Vector<String> entities = new Vector<String>();
    private Vector<ElmDecl> elements = new Vector<ElmDecl>();
    private Vector<AttDecl> attributes = new Vector<AttDecl>();
    private int dtd = 0;
    private int numDtds;

    private int entMaxLen;
    private int attMaxLen;
    private int elmMaxLen;
    private int elmMaxAttNum;

    private String attBuffer = null;


    /** list of key elements to be dumped in a macro in dtd.h */
    private String[] listKeyElms = new String [] {
        "html", "head", "body", "frameset", "style", "script", "meta", "p",
        "title", "pre", "frame", "applet", "a", "form", "iframe", "img", "map",
        "ul", "ol", "li", "table", "tr", "th", "td", "thead", "tbody", "object",
        "big", "small", "sub", "sup", "input", "select", "textarea", "label",
        "button", "fieldset", "isindex", "center", "u", "s", "strike",
        "ins", "del", "area", "font", "basefont", "dir", "menu",
        "ruby", "rb", "rbc", "rp", "rt", "rtc", "bdo", "div", "span",
        "dl", "hr", "caption", "base"};

    /** list of key attributes to be dumped in a macro in dtd.h */
    private String[] listKeyAtts = new String [] {
      "xml:space", "http-equiv", "content", "xmlns"};

    public DTDDeclHandler(int numDtds) {

        this.numDtds = numDtds;
    }

    public void elementDecl(String name, String model)
        throws SAXException
    {
//      System.err.println("elementDecl: ");
//      System.err.println("   "+name);
//      System.err.println("   "+model);

        ElmDecl elm = ElmDecl.searchElm(name, elements);

        if (elm == null) {
            elm = new ElmDecl(name, numDtds);
            elm.id = elements.size();
            elements.add(elm);
        }
        elm.setData(dtd, model);
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

        /* create and link the attribute */
        AttDecl key;

        try {
            key = new AttDecl(aName, type, valueDefault, value, dtd);
        } catch (Exception ex) {
            throw new SAXException("attributeDecl: " + ex.getMessage());
        }

        AttDecl att = AttDecl.searchAtt(attributes, key);
        if (att == null) {
            key.id = attributes.size();
            attributes.add(key);
            att = key;
        } else {
            att.dtd = att.dtd | (1 << dtd);
        }

        /* link the attribute to the element */
        ElmDecl elm = ElmDecl.searchElm(eName, elements);
        if (elm == null) {
            /* Sometimes ATTLIST appears before than ELEMENT (xhtml 1.1) */
            elm = new ElmDecl(eName, numDtds);
            elm.id = elements.size();
            elements.add(elm);
        }

        elm.attList[dtd].add(new Integer(att.id));
    }

    public void internalEntityDecl(String name, String value)
        throws SAXException
    {
//      System.err.println("internalEntityDecl: ");
//      System.err.println("   "+name+": "+value+" ["+"]");

        /* store only the name of the entity */
        if (name.charAt(0) != '%')
          if (entities.indexOf(name) == -1)
            entities.add(name);
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

    /** set DTD id for the following invocations */
    protected void setDtd(int num) {
        dtd = num;
    }

    /** write dtd.c and dtd.h */
    protected void writeFiles(String outputPath) {
        finishAtts(attributes);
        finishElms(elements);

        try {
            // print dtd.c
            FileOutputStream outStr = new FileOutputStream(outputPath + "/dtd.c");
            PrintWriter out= new PrintWriter(outStr);
            System.out.println("Writing " + outputPath + "/dtd.c");
            printHeader(out);
            printEntities(out);
            printAtts(out);
            printElms(out);
            out.close();

            // print dtd.h
            outStr = new FileOutputStream(outputPath + "/dtd.h");
            out = new PrintWriter(outStr);
            System.out.println("Writing " + outputPath + "/dtd.h");
            writeDtdH(out);
            out.close();
        } catch (java.io.FileNotFoundException ex) {
            System.err.println(ex.getMessage());
        }
    }

    /**
     * create the buffer for attributes
     *
     */
    private void finishAtts(Vector<AttDecl> attributes) {
        attBuffer = "";
        int maxLen = 0;

        for (int i=0; i < attributes.size(); i++) {
            AttDecl att = attributes.get(i);

            if (att.name.length() > maxLen) maxLen = att.name.length();

            if (att.type == AttDecl.ATTTYPE_ENUMERATED) {
                // insert enumerated type into the buffer
                att.type = attBuffer.length();
                attBuffer = attBuffer + att.stringType + '\0';
            }

            if ((att.required == AttDecl.DEFDECL_DEFAULT)
                || (att.required == AttDecl.DEFDECL_FIXED)) {
                att.defaultId = attBuffer.length();
                attBuffer = attBuffer + att.defaultString + '\0';
            }
        }

        attMaxLen = maxLen + 1;
    }

    /**
     * creates the buffer for elements
     *
     */
    private void finishElms(Vector<ElmDecl> elements) {
        int elmNum = elements.size();
        int maxAttNum = 0;
        int maxLen = 0;

        for (int i = 0; i < elmNum; i++) {
            ElmDecl elm = elements.elementAt(i);
            for (int nDtd = 0; nDtd < numDtds; nDtd++) {
                if ((elm.contentType[nDtd] == ElmDecl.CONTENT_MIXED)
                    ||(elm.contentType[nDtd] == ElmDecl.CONTENT_CHILDREN)) {

                    elm.contentPtr[nDtd]=
                        ElmDecl.encodeContent(elements, elm.contentType[nDtd],
                                              elm.content[nDtd]);
                }

                // number of attributes
                if (elm.attList[nDtd].size() > maxAttNum)
                    maxAttNum = elm.attList[nDtd].size();
            } // for nDtd

            // name length
            if (elm.name.length() > maxLen) maxLen = elm.name.length();
        } // for i

        elmMaxLen = maxLen + 1;
        elmMaxAttNum = maxAttNum + 1;
    }


    /**
     * write the header of dtd.c: comments & includes
     *
     */
    private void printHeader(PrintWriter out) {
        DTDCoder.writeFileHeader(out);
        out.println("#include \"dtd.h\"");
        out.println("");
    }

    private void printEntities(PrintWriter out) {

        /* look for the longest name to set the array length */
        int max= 0;
        for (int i = 0; i < entities.size(); i++)
            if (entities.elementAt(i).length() > max)
                max = entities.elementAt(i).length();

        out.println("char ent_list[" + entities.size() + "][" + (max + 1) + "]= {");

        /* compute the hash value for each entity */
        int[] hashValue = new int[entities.size()];

        for (int i = 0; i < entities.size(); i++) {
            int hash = entities.get(i).hashCode() % 256;
            if (hash >= 0) hashValue[i] = hash;
            else hashValue[i] = hash + 256;
        }

        boolean comma = false;
        int[] hashTable = new int[256];

        for (int k = 0, pos = 0; k < 256; k++) {
            hashTable[k] = pos;
            for (int i = 0; i < entities.size(); i++) {
                if (hashValue[i] == k) {
                    if (comma) out.println(",");
                    else out.println("");
                    comma = true;
                    pos++;
                    out.print ("    \"" + entities.elementAt(i) + "\"  /*" +
                               hashValue[i] + '*' + "/");
                }
            }
        }
        out.println("");
        out.println("};");

        // print the hash table
        out.println("/* hash table for entities */");
        out.print("int ent_hash[257]= {");

        for (int i=0; i < 256; i++) {
            if ((i & 0xf) == 0) out.println("");
            String ns = String.valueOf(hashTable[i]);
            while (ns.length() < 3) ns = " " + ns;

            if (i == 0) out.print( " " + ns);
            else out.print("," + ns);
        }
        out.println("\n," + entities.size() + "};\n");
        entMaxLen = max + 1;
    }

    private void printElms(PrintWriter out) {
        out.println("\n/* elements */");
        out.println("elm_data_t elm_list[" + elements.size() + "]= {");

        boolean first= true;
        for (int i = 0; i < elements.size(); i++) {
            ElmDecl elm = elements.get(i);

            if (!first) out.println(",");
            else first = false;

            out.print("    {\"" + elm.name + "\",{" + elm.contentType[0]);
            for (int k = 1; k < numDtds; k++)
                out.print("," + elm.contentType[k]);

            out.print("},{" + elm.contentPtr[0]);
            for (int k = 1; k < numDtds; k++)
                out.print("," + elm.contentPtr[k]);

            out.println("},");
            out.print("        {\n");

            // print the attribute list
            for (int k = 0; k < numDtds; k++) {
                int l;
                out.print("           {");
                for (l = 0; l < elm.attList[k].size(); l++) {
                    out.print(elm.attList[k].elementAt(l) + ",");
                    if ((l == 15) || (l == 31)) out.print("\n            ");
                }
                for ( ; l < (elmMaxAttNum - 1); l++) {
                    out.print("-1,");
                    if ((l == 15) || (l == 31)) out.print("\n            ");
                }
                out.print("-1}");
                if (k < numDtds - 1) out.println(",");
                else out.print("\n       },");
            }
            out.println(" " + elm.dtd + "}");
        } // for i

        out.println("};\n");

        // content buffer
        out.println("/* buffer for element content specs */");
        out.print("\nunsigned char elm_buffer[" + ElmDecl.elmBuffer.length() +
                  "]= {");

        char[] chars= ElmDecl.elmBuffer.toCharArray();

        for (int i = 0; i < chars.length; i++) {
            if ((i & 0xf) == 0) out.println("");
            int num = chars[i];
            String ns = String.valueOf(num);
            while (ns.length() < 3) ns = " " + ns;

            if (i == 0) out.print(" " + ns);
            else out.print("," + ns);
        }
        out.println("\n};");
    }

    private void printAtts(PrintWriter out) {
        out.println("\n/* atributos */");
        out.println("att_data_t att_list[" + attributes.size() + "]= {");

        int num = attributes.size();
        for (int i = 0; i < num; i++) {
            AttDecl att = attributes.elementAt(i);

            out.print("    {\"" + att.name + "\"," + att.type + ","
                      + att.required +
                      "," + att.defaultId + "," + att.dtd + "}");
            if ((i+1) == num) out.println("");
            else out.println(",");
        } // for i

        out.println("};\n");
        out.println("/* data buffer for attribute specs */");
        out.print("\nunsigned char att_buffer[" + attBuffer.length() + "]= {");

        char[] chars= attBuffer.toCharArray();
        for (int i = 0; i < chars.length; i++) {
            if ((i & 0xf) == 0) out.println("");
            num = chars[i];
            String ns = String.valueOf(num);
            while (ns.length() < 3) ns = " " + ns;

            if (i==0) out.print(" " + ns);
            else out.print("," + ns);
        }
        out.println("\n};");
    }

    private void writeDtdH(PrintWriter out) {
        SimpleDateFormat sdf = new SimpleDateFormat("yyyy-MM-dd HH:mm:ssZ");
        String date = sdf.format(new Date()).toString();

        DTDCoder.writeFileHeader(out);

        out.println("#ifndef DTD_H\n#define DTD_H\n");
        out.println("#define DTD_NUM " + numDtds + "\n");
        out.println("#define ATT_NAME_LEN " + attMaxLen);
        out.println("#define ELM_NAME_LEN " + elmMaxLen);
        out.println("#define ENT_NAME_LEN " + entMaxLen + "\n");
        out.println("#define ELM_ATTLIST_LEN " + elmMaxAttNum + "\n");

        out.println("/* files that declare types and other macros */");
        out.println("#include \"dtd_types.h\"");
        out.println("#include \"dtd_names.h\"\n");

        out.println("#define DTD_SNAPSHOT_DATE \"" + date + "\"\n");

        out.println("#define elm_data_num " + elements.size());
        out.println("#define att_data_num " + attributes.size());
        out.println("#define ent_data_num " + entities.size());
        out.println("#define elm_buffer_num " + ElmDecl.elmBuffer.length());
        out.println("#define att_buffer_num " + attBuffer.length());

        out.println("\n/* variables declared in dtd.c */");
        out.println("extern elm_data_t elm_list[];");
        out.println("extern unsigned char elm_buffer[];");
        out.println("extern att_data_t att_list[];");
        out.println("extern unsigned char att_buffer[];\n");

        out.println("extern int ent_hash[257];");
        out.println("extern char ent_list[][" + entMaxLen + "];\n");

        /* key elements and attributes */
        for (int i = 0; i < listKeyElms.length; i++)
            keyElm(listKeyElms[i], out);
        for (int i = 0; i < listKeyAtts.length; i++)
            keyAtt(listKeyAtts[i], out);

        out.println("#endif /* DTD_H */");
    }


    private void keyElm(String name, PrintWriter out) {
        ElmDecl elm = ElmDecl.searchElm(name, elements);

        if (elm != null)
            out.println("#define ELMID_" + name.toUpperCase() + " " + elm.id);
    }

    private void keyAtt(String name, PrintWriter out) {

        Vector<AttDecl> atts = AttDecl.searchAttByName(attributes, name);
        if (atts.size() == 1) {
            AttDecl att = atts.elementAt(0);
            out.println("#define ATTID_" + name.toUpperCase().replace(':','_')
                        .replace('-','_') + " " + att.id);
        } else if (atts.size() > 1) {
            AttDecl att;
            for (int i=0; i < atts.size(); i++) {
                att = atts.elementAt(i);
                out.println("#define ATTID_" + name.toUpperCase().replace(':','_')
                            .replace('-','_') + "_" + i + " " + att.id);
            }
        }
    }
}
