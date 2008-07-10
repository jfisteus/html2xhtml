/*
 * DTDCoder.java
 *
 * (Jesús Arias Fisteus)
 *
 * Codificación de la clase DTDCoder. Genera los ficheros
 * dtd.c, dtd.h, dtd_names.c y dtd_names.h
 * necesarios para el resto del programa,
 * a partir del análisis de los DTD XHTML de entrada.
 *
 * Funciona en combinación con DTDDeclHandler, que recibirá
 * las notificaciones de declaraciones encontradas en el DTD
 *
 * Forma de invocación explicada ejecutándolo sin parámetros
 *
 * $Id: DTDCoder.java,v 1.5 2005/01/29 19:38:44 jaf Exp $
 *
 */

import org.xml.sax.helpers.DefaultHandler;
import org.w3c.dom.Element;
import org.w3c.dom.NodeList;
import org.w3c.dom.Node;
import org.w3c.dom.Document;
import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.SAXParserFactory;
import javax.xml.parsers.SAXParser;
import org.xml.sax.XMLReader;
import org.xml.sax.helpers.XMLReaderFactory;

import java.io.PrintWriter;
import java.io.FileOutputStream;
import java.io.IOException;

/**
 * Clase que invoca el parser sobre ficheros de prueba
 *
 */
public class DTDCoder
    extends DefaultHandler
{

    public DTDData[] procesarIndice(String indice)
    {
        DocumentBuilderFactory factory = DocumentBuilderFactory.newInstance();
        Document doc = null;
        try {
            doc = factory.newDocumentBuilder().parse(indice);
        } catch (Exception e) {
            e.printStackTrace();
            return null;
        }

        Element dtdsElement = doc.getDocumentElement();
        NodeList dtdElements = dtdsElement.getElementsByTagName("dtd");
        DTDData[] dtds = new DTDData[dtdElements.getLength()];
        for (int i = 0; i < dtds.length; i++) {
            Element element = (Element) dtdElements.item(i);
            dtds[i] = new DTDData(element);
        }

        return dtds;
    }


    public void procesarDtd(DTDData dtd, DTDDeclHandler handler)
    {
        System.out.println("Procesando DTD " + dtd.getKey()
                           +  "; plantilla " + dtd.getTemplateFile());

        // código comentado: bug en parser SAX de J2SDK 1.4
        // no funciona cuando se redeclara una entidad, como es
        // el caso del DTD de xhtml 1.1.
        //

        SAXParserFactory factory = SAXParserFactory.newInstance();
        factory.setValidating(true);

        try {
            SAXParser parser = factory.newSAXParser();
            parser.setProperty("http://xml.org/sax/properties/declaration-handler",
                               handler);
            parser.parse(dtd.getTemplateFile(), this);
        } catch (Exception e) {
            e.printStackTrace();
        }

        // alternativa utilizando Xerces
        //
//      try {
//          XMLReader parser = XMLReaderFactory.createXMLReader("org.apache.xerces.parsers.SAXParser");
//          parser.setProperty("http://xml.org/sax/properties/declaration-handler",
//                             handler);
//          parser.parse(dtd.getTemplateFile());
//      } catch (Exception e) {
//          e.printStackTrace();
//      }

    }

    protected void escribirDtdNames(DTDData[] dtds, String dir)
        throws IOException
    {

        // calcula las longitudes máximas
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


        // Escribe dtd_names.h
        PrintWriter out =
            new PrintWriter(new FileOutputStream(dir + "/dtd_names.h"));

        System.out.println("Escribiendo " + dir + "/dtd_names.h");

        out.println("/*");
        out.println(" * fichero generado mediante DTDCoder");
        out.println(" * NO EDITAR");
        out.println(" *");
        out.println(" * asociación entre número de DTD y nombre");
        out.println(" *");
        out.println(" */");
        out.println();
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


        // Escribe dtd_names.c
        out = new PrintWriter(new FileOutputStream(dir + "/dtd_names.c"));

        System.out.println("Escribiendo " + dir + "/dtd_names.c");

        out.println("/*");
        out.println(" * fichero generado mediante DTDCoder");
        out.println(" * NO EDITAR");
        out.println(" *");
        out.println(" * datos asociados a cada DTD");
        out.println(" *");
        out.println(" */");
        out.println();
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
     * Convierte " a \"
     *
     */
    private String regexp(String str)
    {
        return str.replaceAll("\\\"","\\\\\\\"");
    }

    public static void main(String argv[]) {

        if (argv.length != 2) {
            System.err.println("uso: java DTDCoder indice"
                               + " ruta_salida\n");
            System.err.println("indice:");
            System.err.println("  fichero con información básica de cada DTD");
            System.err.println("ruta salida:");
            System.err.println("  genera dtd.c, dtd.h y dtd_names.c "
                               + "en el directorio 'ruta salida'\n");

            System.exit(1);
        }

        DTDCoder coder = new DTDCoder();
        DTDData[] dtds = coder.procesarIndice(argv[0]);
        DTDDeclHandler handler= new DTDDeclHandler(dtds.length);

        for (int i = 0; i < dtds.length; i++) {
            handler.establecerDtd(i);
            coder.procesarDtd(dtds[i], handler);
        }

        // escribe dtd.c y dtd.h
        handler.finalizar(argv[1]);

        try {
            // escribe dtd_names.c y dtd_names.h
            coder.escribirDtdNames(dtds, argv[1]);
        } catch (IOException e) {
            e.printStackTrace();
        }
    }
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
