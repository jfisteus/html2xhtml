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
 * DTDCoder.java
 *
 * Main class that generates dtd.c, dtd.h, dtd_names.c and dtd_names.h
 *
 */

import java.io.FileOutputStream;
import java.io.IOException;
import java.io.PrintWriter;

import javax.xml.parsers.DocumentBuilderFactory;
import org.xml.sax.helpers.XMLReaderFactory;
import org.xml.sax.SAXException;
import org.xml.sax.XMLReader;

import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;
import org.xml.sax.helpers.DefaultHandler;

public class DTDCoder
    extends DefaultHandler
{

    public static void main(String argv[]) {
        if (argv.length != 2) {
            System.out.println("Error: two command line arguments expected:");
            System.err.println("java DTDCoder <index_file>"
                               + " <output_dir>\n");
            System.exit(1);
        }

        DTDCoder coder = new DTDCoder();
        DTDData[] dtds = coder.processIndexFile(argv[0]);
        DTDDeclHandler handler= new DTDDeclHandler(dtds.length);
        XHTMLEntityResolver resolver = new XHTMLEntityResolver();

        for (int i = 0; i < dtds.length; i++) {
            handler.setDtd(i);
            coder.processDtd(dtds[i], handler, resolver);
        }

        // escribe dtd.c y dtd.h
        handler.writeFiles(argv[1]);

        try {
            // write dtd_names.c y dtd_names.h
            coder.writeDtdNames(dtds, argv[1]);
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    public DTDData[] processIndexFile(String indice)
    {
        DocumentBuilderFactory factory = DocumentBuilderFactory.newInstance();
        Document doc = null;
        try {
            doc = factory.newDocumentBuilder().parse(indice);
        } catch (Exception e) {
            e.printStackTrace();
            return null;
        }

        Element rootElement = doc.getDocumentElement();
        NodeList dtdElements = rootElement.getElementsByTagName("dtd");
        DTDData[] dtds = new DTDData[dtdElements.getLength()];
        for (int i = 0; i < dtds.length; i++) {
            Element element = (Element) dtdElements.item(i);
            dtds[i] = new DTDData(element);
        }

        return dtds;
    }


    public void processDtd(DTDData dtd, DTDDeclHandler handler,
                           XHTMLEntityResolver resolver)
    {
        XMLReader reader;
        System.out.println("Processing DTD " + dtd.getKey()
                           +  "; from template " + dtd.getTemplateFile());

         try {
             resolver.setBaseURL(dtd.getDtdUrl());
             reader = XMLReaderFactory.createXMLReader();
             reader.setProperty("http://xml.org/sax/properties/declaration-handler",
                                handler);
             reader.setEntityResolver(resolver);
             reader.parse(dtd.getTemplateFile());
         } catch (SAXException se) {
             se.printStackTrace();
         } catch (IOException ioe) {
             ioe.printStackTrace();
         }


    }

    protected void writeDtdNames(DTDData[] dtds, String dir)
        throws IOException
    {

        // compute maximum lengths
        int publicIdSize = 0;
        int dtdUrlSize = 0;
        int keySize = 0;
        int nameSize = 0;
        for (int i = 0; i < dtds.length; i++) {
            int length;
            length = 1 + dtds[i].getPublicId().length();
            if (length > publicIdSize) {
                publicIdSize = length;
            }
            length = 1 + dtds[i].getDtdUrl().length();
            if (length > dtdUrlSize) {
                dtdUrlSize = length;
            }
            length = 1 + dtds[i].getKey().length();
            if (length > keySize) {
                keySize = length;
            }
            length = 1 + dtds[i].getName().length();
            if (length > nameSize) {
                nameSize = length;
            }
        }


        // Write dtd_names.h
        PrintWriter out =
            new PrintWriter(new FileOutputStream(dir + "/dtd_names.h"));

        System.out.println("Writing " + dir + "/dtd_names.h");
        writeFileHeader(out);

        out.println("#define XHTML_NUM_DTDS " + dtds.length);
        out.println();
        for (int i = 0; i < dtds.length; i++) {
            out.println("#define " + dtds[i].getConstantName()
                        + " " + i);
        }
        out.println();
        out.println("#define DOC_STR_LEN " + publicIdSize);
        out.println("#define DTD_STR_LEN " + dtdUrlSize);
        out.println("#define DTD_KEY_LEN " + keySize);
        out.println("#define DTD_NAM_LEN " + nameSize);
        out.close();


        // Write dtd_names.c
        out = new PrintWriter(new FileOutputStream(dir + "/dtd_names.c"));

        System.out.println("Escribiendo " + dir + "/dtd_names.c");
        writeFileHeader(out);

        out.println("#include \"dtd.h\"");
        out.println("#include \"dtd_names.h\"");
        out.println();

        out.println("char doctype_string[DTD_NUM][DOC_STR_LEN]= {");
        for (int i = 0; i < dtds.length; i++) {
            out.print("    \"" + regexp(dtds[i].getPublicId()) + "\"");
            if (i < dtds.length - 1) {
                out.println(",");
            } else {
                out.println();
            }
        }
        out.println("};");
        out.println();

        out.println("char dtd_string[DTD_NUM][DTD_STR_LEN]= {");
        for (int i = 0; i < dtds.length; i++) {
            out.print("    \"" + dtds[i].getDtdUrl() + "\"");
            if (i < dtds.length - 1) {
                out.println(",");
            } else {
                out.println();
            }
        }
        out.println("};");
        out.println();

        out.println("char dtd_key[DTD_NUM][DTD_KEY_LEN]= {");
        for (int i = 0; i < dtds.length; i++) {
            out.print("    \"" + dtds[i].getKey() + "\"");
            if (i < dtds.length - 1) {
                out.println(",");
            } else {
                out.println();
            }
        }
        out.println("};");
        out.println();

        out.println("char dtd_name[DTD_NUM][DTD_NAM_LEN]= {");
        for (int i = 0; i < dtds.length; i++) {
            out.print("    \"" + dtds[i].getName() + "\"");
            if (i < dtds.length - 1) {
                out.println(",");
            } else {
                out.println();
            }
        }
        out.println("};");
        out.println();

        out.close();
    }

    /**
     * Convert " into \"
     *
     */
    private String regexp(String str)
    {
        return str.replaceAll("\\\"","\\\\\\\"");
    }

    public static void writeFileHeader(PrintWriter out) {
        out.println("/*");
        out.println(" * This file has been created automatically by DTDCoder.");
        out.println(" * Do not edit, use DTDCoder instead.");
        out.println(" *");
        out.println(" */");
        out.println();
    }

    class DTDData
    {
        private String key;
        private String name;
        private String constantName;
        private String publicId;
        private String dtdUrl;
        private String templateFile;

        public DTDData(Element element)
            throws IllegalArgumentException
        {
            if (!"dtd".equals(element.getNodeName())) {
                throw new IllegalArgumentException("Element 'dtd' expected");
            }

            key = getElementContent("key", element);
            name = getElementContent("name", element);
            constantName = getElementContent("constantName", element);
            publicId = getElementContent("publicId", element);
            dtdUrl = getElementContent("dtdUrl", element);
            templateFile = getElementContent("templateFile", element);
        }

        public String getKey()
        {
            return key;
        }

        public String getName()
        {
            return name;
        }

        public String getConstantName()
        {
            return constantName;
        }

        public String getPublicId()
        {
            return publicId;
        }

        public String getDtdUrl()
        {
            return dtdUrl;
        }

        public String getTemplateFile()
        {
            return templateFile;
        }

        private String getElementContent(String elementName, Element baseElement)
            throws IllegalArgumentException
        {
            NodeList children = baseElement.getElementsByTagName(elementName);
            if (children.getLength() == 1) {
                Element element = (Element) children.item(0);
                Node content = element.getFirstChild();

                if (content.getNodeType() == Node.TEXT_NODE) {
                    return content.getNodeValue();
                } else {
                    throw new IllegalArgumentException("Bad content in "
                                                       + elementName);
                }
            } else {
                throw new IllegalArgumentException("Element " + elementName
                                                   + " not found");
            }
        }
    }
}
