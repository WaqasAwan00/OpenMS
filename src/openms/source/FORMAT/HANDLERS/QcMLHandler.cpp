// --------------------------------------------------------------------------
//                   OpenMS -- Open-Source Mass Spectrometry
// --------------------------------------------------------------------------
// Copyright The OpenMS Team -- Eberhard Karls University Tuebingen,
// ETH Zurich, and Freie Universitaet Berlin 2002-2013.
//
// This software is released under a three-clause BSD license:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of any author or any participating institution
//    may be used to endorse or promote products derived from this software
//    without specific prior written permission.
// For a full list of authors, refer to the file AUTHORS.
// --------------------------------------------------------------------------
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL ANY OF THE AUTHORS OR THE CONTRIBUTING
// INSTITUTIONS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// --------------------------------------------------------------------------
// $Maintainer: Mathias Walzer $
// $Authors: Mathias Walzer$
// --------------------------------------------------------------------------

#include <OpenMS/FORMAT/HANDLERS/QcMLHandler.h>
#include <OpenMS/SYSTEM/File.h>
#include <OpenMS/CHEMISTRY/ModificationsDB.h>
#include <OpenMS/METADATA/DataProcessing.h>
#include <OpenMS/DATASTRUCTURES/DateTime.h>
#include <set>
#include <vector>
#include <map>
#include <iostream>
#include <algorithm>

using namespace std;

namespace OpenMS
{
  namespace Internal
  {
    QcMLHandler::QcMLHandler(const MSQuantifications& msq, const String& filename, const String& version, const ProgressLogger& logger) :
      XMLHandler(filename, version),
      logger_(logger),
      msq_(0),
      cmsq_(&msq)
    {
      cv_.loadFromOBO("MS", File::find("/CV/psi-ms.obo")); //TODO unimod -> then automatise CVList writing
    }

    QcMLHandler::QcMLHandler(MSQuantifications& msq, /* FeatureMap& feature_map, */ const String& filename, const String& version, const ProgressLogger& logger) :
      XMLHandler(filename, version),
      logger_(logger),
      msq_(&msq),
      cmsq_(0)
    {
      cv_.loadFromOBO("MS", File::find("/CV/psi-ms.obo"));
    }

    QcMLHandler::~QcMLHandler()
    {
    }

    void QcMLHandler::startElement(const XMLCh* const /*uri*/, const XMLCh* const /*local_name*/, const XMLCh* const qname, const xercesc::Attributes& attributes)
    {
      // not implemented
    }

    void QcMLHandler::characters(const XMLCh* const chars, const XMLSize_t /*length*/)
    {
      //not implemented
    }

    void QcMLHandler::endElement(const XMLCh* const /*uri*/, const XMLCh* const /*local_name*/, const XMLCh* const qname)
    {
      //not implemented
    }

    void QcMLHandler::handleCVParam_(const String& parent_parent_tag, const String& parent_tag, const String& accession, const String& name, const String& value, const xercesc::Attributes& /* attributes */, const String& /* cv_ref */, const String& /* unit_accession */)
    {
      //Abort on unknown terms
      if (!cv_.exists(accession))
      {
        //in 'sample' several external CVs are used (Brenda, GO, ...). Do not warn then.
        if (parent_tag != "sample")
        {
          warning(LOAD, String("Unknown cvParam '") + accession + "' in tag '" + parent_tag + "'.");
          return;
        }
      }
      else
      {
        const ControlledVocabulary::CVTerm& term = cv_.getTerm(accession);
        //obsolete CV terms
        if (term.obsolete)
        {
          warning(LOAD, String("Obsolete CV term '") + accession + " - " + cv_.getTerm(accession).name + "' used in tag '" + parent_tag + "'.");
        }
        //check if term name and parsed name match
        String parsed_name = name;
        parsed_name.trim();
        String correct_name = term.name;
        correct_name.trim();
        if (parsed_name != correct_name)
        {
          warning(LOAD, String("Name of CV term not correct: '") + term.id + " - " + parsed_name + "' should be '" + correct_name + "'");
        }
        if (term.obsolete)
        {
          warning(LOAD, String("Obsolete CV term '") + accession + " - " + cv_.getTerm(accession).name + "' used in tag '" + parent_tag + "'.");
        }
        //values used in wrong places and wrong value types
        if (value != "")
        {
          if (term.xref_type == ControlledVocabulary::CVTerm::NONE)
          {
            //Quality CV does not state value type :(
            if (!accession.hasPrefix("PATO:"))
            {
              warning(LOAD, String("The CV term '") + accession + " - " + cv_.getTerm(accession).name + "' used in tag '" + parent_tag + "' must not have a value. The value is '" + value + "'.");
            }
          }
          else
          {
            switch (term.xref_type)
            {
            //string value can be anything
            case ControlledVocabulary::CVTerm::XSD_STRING:
              break;

            //int value => try casting
            case ControlledVocabulary::CVTerm::XSD_INTEGER:
            case ControlledVocabulary::CVTerm::XSD_NEGATIVE_INTEGER:
            case ControlledVocabulary::CVTerm::XSD_POSITIVE_INTEGER:
            case ControlledVocabulary::CVTerm::XSD_NON_NEGATIVE_INTEGER:
            case ControlledVocabulary::CVTerm::XSD_NON_POSITIVE_INTEGER:
              try
              {
                value.toInt();
              }
              catch (Exception::ConversionError&)
              {
                warning(LOAD, String("The CV term '") + accession + " - " + cv_.getTerm(accession).name + "' used in tag '" + parent_tag + "' must have an integer value. The value is '" + value + "'.");
                return;
              }
              break;

            //double value => try casting
            case ControlledVocabulary::CVTerm::XSD_DECIMAL:
              try
              {
                value.toDouble();
              }
              catch (Exception::ConversionError&)
              {
                warning(LOAD, String("The CV term '") + accession + " - " + cv_.getTerm(accession).name + "' used in tag '" + parent_tag + "' must have a floating-point value. The value is '" + value + "'.");
                return;
              }
              break;

            //date string => try conversion
            case ControlledVocabulary::CVTerm::XSD_DATE:
              try
              {
                DateTime tmp;
                tmp.set(value);
              }
              catch (Exception::ParseError&)
              {
                warning(LOAD, String("The CV term '") + accession + " - " + cv_.getTerm(accession).name + "' used in tag '" + parent_tag + "' must be a valid date. The value is '" + value + "'.");
                return;
              }
              break;

            default:
              warning(LOAD, String("The CV term '") + accession + " - " + cv_.getTerm(accession).name + "' used in tag '" + parent_tag + "' has the unknown value type '" + ControlledVocabulary::CVTerm::getXRefTypeName(term.xref_type) + "'.");
              break;
            }
          }
        }
        //no value, although there should be a numerical value
        else if (term.xref_type != ControlledVocabulary::CVTerm::NONE && term.xref_type != ControlledVocabulary::CVTerm::XSD_STRING)
        {
          warning(LOAD, String("The CV term '") + accession + " - " + cv_.getTerm(accession).name + "' used in tag '" + parent_tag + "' should have a numerical value. The value is '" + value + "'.");
          return;
        }
      }

      if (parent_tag == "DataType" && parent_parent_tag == "Column")
      {
        if (current_count_ >= current_col_types_.size())
        {
          current_col_types_.resize(current_count_ + 1, "");
        }
        current_col_types_[current_count_] = accession; //TODO real cv handling here (i.e. translate name into decision string for the "row-loop")
      }
      else if (parent_parent_tag == "Label")
      {
        //TODO
        if (accession == "MOD:01522")
          current_assay_.mods_.push_back(std::make_pair<String, double>("114", double(114)));
        else if (accession == "MOD:01523")
          current_assay_.mods_.push_back(std::make_pair<String, double>("115", double(115)));
        else if (accession == "MOD:01524")
          current_assay_.mods_.push_back(std::make_pair<String, double>("116", double(116)));
        else if (accession == "MOD:01525")
          current_assay_.mods_.push_back(std::make_pair<String, double>("117", double(117)));

      }
      else
        warning(LOAD, String("Unhandled cvParam '") + name + "' in tag '" + parent_tag + "'.");
    }

    void QcMLHandler::handleUserParam_(const String& parent_parent_tag, const String& parent_tag, const String& name, const String& type, const String& value)
    {
      //create a DataValue that contains the data in the right type
      DataValue data_value;
      //float type
      if (type == "xsd:double" || type == "xsd:float")
      {
        data_value = DataValue(value.toDouble());
      }
      //integer type
      else if (type == "xsd:byte" || type == "xsd:decimal" || type == "xsd:int" || type == "xsd:integer" || type == "xsd:long" || type == "xsd:negativeInteger" || type == "xsd:nonNegativeInteger" || type == "xsd:nonPositiveInteger" || type == "xsd:positiveInteger" || type == "xsd:short" || type == "xsd:unsignedByte" || type == "xsd:unsignedInt" || type == "xsd:unsignedLong" || type == "xsd:unsignedShort")
      {
        data_value = DataValue(value.toInt());
      }
      //everything else is treated as a string
      else
      {
        data_value = DataValue(value);
      }

      if (parent_parent_tag == "")
      {
        //~ TODO: dummy
        warning(LOAD, String("The user param '") + name + "' used in tag '" + parent_tag + "' has no valid grand parent.'");
      }
      //find the right MetaInfoInterface
      if (parent_tag == "ProcessingMethod")
      {
        //~ value is softwarename - will get handled elsewhere
        int x = std::distance(DataProcessing::NamesOfProcessingAction, std::find(DataProcessing::NamesOfProcessingAction, DataProcessing::NamesOfProcessingAction + DataProcessing::SIZE_OF_PROCESSINGACTION, name));
        DataProcessing::ProcessingAction a = static_cast<DataProcessing::ProcessingAction>(x); // ugly and depends on NamesOfProcessingAction^=ProcessingAction-definitions - see TODO rewrite DataProcessing!
        current_pas_.insert(a);
      }
      else if (parent_tag == "Software")
      {
        if (value == "")
        {
          current_sws_[current_id_].setName(name);
        }
        else
        {
          current_sws_[current_id_].setMetaValue(name, data_value);
        }
      }
      else if (parent_tag == "AnalysisSummary")
      {
        if (name == "QuantType")
        {
          const std::string* match = std::find(MSQuantifications::NamesOfQuantTypes, MSQuantifications::NamesOfQuantTypes + MSQuantifications::SIZE_OF_QUANT_TYPES, value);
          int i = (MSQuantifications::NamesOfQuantTypes + MSQuantifications::SIZE_OF_QUANT_TYPES == match) ? -1 : std::distance(MSQuantifications::NamesOfQuantTypes, match);
          MSQuantifications::QUANT_TYPES quant_type = static_cast<MSQuantifications::QUANT_TYPES>(i); //this is so not nice and soooo unsafe why enum in the first place?!
          msq_->setAnalysisSummaryQuantType(quant_type);
        }
        else
        {
          msq_->getAnalysisSummary().user_params_.setValue(name, data_value);
        }
      }
      else if (parent_tag == "RatioCalculation")
      {
        r_rtemp_[current_id_].description_.push_back(name);
      }
      else if (parent_tag == "Feature")
      {
        if (name == "feature_index")
        {
          f_f_obj_[current_id_].setUniqueId(UInt64(value.toInt()));
        }
        else if (name == "map_index")
        {
          f_f_obj_[current_id_].setMapIndex(UInt64(value.toInt()));
        }
      }
      else
        warning(LOAD, String("Unhandled userParam '") + name + "' in tag '" + parent_tag + "'.");
    }

    void QcMLHandler::writeTo(std::ostream& os)
    {
      //~ TODO logger_.startProgress(0,exp.size(),"storing QcML file");
      String line; //everyone walk the line!!!
      //~ std::vector<UInt64> rid;

      //header
      os << "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>\n";
      os << "<?xml-stylesheet type=\"text/xml\" href=\"#stylesheet\"?>\n";
      os << "<!DOCTYPE catelog [\n"
         << "  <!ATTLIST xsl:stylesheet\n"
         << "  id  ID  #REQUIRED>\n"
         << "  ]>\n";
      os << "<qcMLType>\n"; //TODO creation date into schema!!
      os << "<xsl:stylesheet id=\"stylesheet\" version=\"1.0\" xmlns:xsl=\"http://www.w3.org/1999/XSL/Transform\">
         << "<xsl:template match=\"/\">
         << "  <html>
         << "  <body>
         << "		<h2>The Quality Parameters</h2>
         << "			<table border=\"1\">
         << "				<tr bgcolor=\"#9acd32\">
         << "					<th>Parameter</th>
         << "					<th>Value</th>
         << "				</tr>
         << "				<xsl:for-each select=\"qcMLType/runQuality/qualityParameter\">
         << "				<tr>
         << "					<td><xsl:value-of select=\"@name\" /></td>
         << "					<td><xsl:value-of select=\"@value\" /></td>
         << "				</tr>
         << "				</xsl:for-each>
         << "			</table><br/>
         << "		<h2>The Quality Plots</h2>
         << "      <xsl:for-each select=\"qcMLType/runQuality/attachment\">
         << "        <img>
         << "      <xsl:attribute name=\"src\">
         << "          data:image/png;base64,<xsl:value-of select=\"binary\" />
         << "         </xsl:attribute>
        </img> <br/>
      </xsl:for-each>
  </body>
  </html>
</xsl:template>
</xsl:stylesheet>"


      //CVList
      os << "<CvList>\n";
      os << " \t<Cv id=\"PSI-MS\" fullName=\"Proteomics Standards Initiative Mass Spectrometry Vocabularies\"  uri=\"http://psidev.cvs.sourceforge.net/viewvc/*checkout*/psidev/psi/psi-ms/mzML/controlledVocabulary/psi-ms.obo\" version=\"3.41.0\"/>\n";
      os << "\t<Cv id=\"PSI-MOD\" fullName=\"Proteomics Standards Initiative Protein Modifications Vocabularies\" uri=\"http://psidev.cvs.sourceforge.net/psidev/psi/mod/data/PSI-MOD.obo\" version=\"1.2\"/>\n";
      os << "\t<Cv id=\"UO\" fullName=\"Unit Ontology\" uri=\"http://obo.cvs.sourceforge.net/*checkout*/obo/obo/ontology/phenotype/unit.obo\"/>\n";
      os << "</CvList>\n";

      //AnalysisSummary
      os << "\t<AnalysisSummary>\n";
      cmsq_->getAnalysisSummary().quant_type_;
      //~ os << "\t\t<userParam name=\"QuantType\" value=\"";
      //~ os << String(MSQuantifications::NamesOfQuantTypes[cmsq_->getAnalysisSummary().quant_type_]);
      switch (cmsq_->getAnalysisSummary().quant_type_)
      {
      case 0:
        os << "\t\t<cvParam cvRef=\"PSI-MS\" accession=\"MS:1002018\" name=\"MS1 label-based analysis\"/>\n";
        os << "\t\t<cvParam cvRef=\"PSI-MS\" accession=\"MS:1001837\" name=\"SILAC quantitation analysis\"/>\n";
        os << "\t\t<cvParam cvRef=\"PSI-MS\" accession=\"MS:1002001\" name=\"MS1 label-based raw feature quantitation\" value=\"true\"/>\n";
        os << "\t\t<cvParam cvRef=\"PSI-MS\" accession=\"MS:1002002\" name=\"MS1 label-based peptide level quantitation\" value=\"true\"/>\n";
        os << "\t\t<cvParam cvRef=\"PSI-MS\" accession=\"MS:1002003\" name=\"MS1 label-based protein level quantitation\" value=\"false\"/>\n";
        os << "\t\t<cvParam cvRef=\"PSI-MS\" accession=\"MS:1002004\" name=\"MS1 label-based proteingroup level quantitation\" value=\"false\"/>\n";
        break;

      case 1:
        os << "\t\t<cvParam cvRef=\"PSI-MS\" accession=\"MS:1002023\" name=\"MS2 tag-based analysis\"/>\n";
        os << "\t\t<cvParam cvRef=\"PSI-MS\" accession=\"MS:1002024\" name=\"MS2 tag-based analysis feature level quantitation\" value=\"true\"/>\n";
        os << "\t\t<cvParam cvRef=\"PSI-MS\" accession=\"MS:1002025\" name=\"MS2 tag-based peptide level quantitation\" value=\"true\"/>\n";
        os << "\t\t<cvParam cvRef=\"PSI-MS\" accession=\"MS:1002026\" name=\"MS2 tag-based analysis protein level quantitation\" value=\"false\"/>\n";
        os << "\t\t<cvParam cvRef=\"PSI-MS\" accession=\"MS:1002027\" name=\"MS2 tag-based analysis protein group level quantitation\" value=\"false\"/>\n";
        break;

      case 2:
        break; //no tool yet

      case 3:
        break; //why SIZE_OF_QUANT_TYPES anyway?
      }
      //~ writeUserParam_(dataprocessinglist_tag, cmsq_->getAnalysisSummary().getUserParams(), UInt(2));
      //~ writeCVParams_(dataprocessinglist_tag, (cmsq_->getAnalysisSummary().getCVTerms(), UInt(2));
      os << "\n\t</AnalysisSummary>\n";

      //Software & DataProcessing
      String softwarelist_tag;
      softwarelist_tag += "\t<SoftwareList>\n";

      String dataprocessinglist_tag;
      dataprocessinglist_tag += "\t<DataProcessingList>\n";
      // TODO Software DefaultTag for each file: OpenMS
      Size order_d = 0;

      String idfile_tag, idfile_ref, searchdb_ref;

      std::vector<DataProcessing> pl = cmsq_->getDataProcessingList();
      for (std::vector<DataProcessing>::const_iterator dit = pl.begin(); dit != pl.end(); ++dit)
      {
        //~ for (std::vector<DataProcessing>::const_iterator dit = cmsq_->getDataProcessingList().begin(); dit != cmsq_->getDataProcessingList().end(); ++dit) // soome wierd bug is making this impossible resulting in segfault - to tired to work this one out right now
        if (dit->getSoftware().getName() == "IDMapper" && !cmsq_->getConsensusMaps().front().getProteinIdentifications().empty())
        {
          searchdb_ref = "sdb_" + String(UniqueIdGenerator::getUniqueId());
          idfile_ref = "idf_" + String(UniqueIdGenerator::getUniqueId());
          String idfile_name = dit->getMetaValue("parameter: id");

          idfile_tag += "\t\t<IdentificationFiles>\n";
          idfile_tag += "\t\t\t<IdentificationFile id=\"" + idfile_ref + "\" name=\"" + idfile_name + "\" location=\"" + idfile_name + "\" searchDatabase_ref=\"" + searchdb_ref + "\"/>\n";
          idfile_tag += "\t\t</IdentificationFiles>\n";

          idfile_tag += "\t\t<SearchDatabase id=\"" + searchdb_ref + "\" location=\"" + cmsq_->getConsensusMaps().front().getProteinIdentifications().front().getSearchParameters().db_version + "\">\n\t\t\t<DatabaseName>\n\t\t\t\t<userParam name=\"db_version\" value=\"" + cmsq_->getConsensusMaps().front().getProteinIdentifications().front().getSearchParameters().db_version + "\" />\n\t\t\t</DatabaseName>\n\t\t</SearchDatabase>\n";
        }

        String sw_ref;
        sw_ref = "sw_" + String(UniqueIdGenerator::getUniqueId());
        softwarelist_tag += "\t\t<Software id=\"" +  sw_ref + "\" version=\"" + String(dit->getSoftware().getVersion()) + "\">\n";
        writeCVParams_(softwarelist_tag, dit->getSoftware().getCVTerms(), UInt(3)); // TODO fix up the tools with their cvparams and make them write it in the softwarelist!
        if (dit->getSoftware().getCVTerms().empty())
        {
          softwarelist_tag += "\t\t\t<userParam name=\"" + String(dit->getSoftware().getName()) + "\"/>\n";
        }
        if (dit->getSoftware().getName() == "ITRAQAnalyzer")
        {
          softwarelist_tag += "\t\t\t<cvParam cvRef=\"PSI-MS\" accession=\"MS:1001831\" name=\"ITRAQAnalyzer\"/>\n";
        }
        softwarelist_tag += "\t\t</Software>\n";
        ++order_d;
        dataprocessinglist_tag += "\t\t<DataProcessing id=\"dp_" + String(UniqueIdGenerator::getUniqueId()) + "\" software_ref=\"" + sw_ref + "\" order=\"" + String(order_d) + "\">\n";
        Size order_c = 0;
        for (std::set<DataProcessing::ProcessingAction>::const_iterator pit = dit->getProcessingActions().begin(); pit != dit->getProcessingActions().end(); ++pit)
        {
          //~ TODO rewrite OpenMS::DataProcessing
          //~ TODO add CVTermList/MetaInfoInterfaceObject to DataProcessing and ParamGroup/Order to "ProcessingAction" or document implicit ordering
          ++order_c;
          dataprocessinglist_tag += "\t\t\t<ProcessingMethod order=\"" + String(order_c) + "\">\n";
          //~ writeUserParam_(dataprocessinglist_tag, pit->getUserParams(), UInt(4));  //writeUserParam_(String& s, const MetaInfoInterface& meta, UInt indent)
          //~ writeCVParams_(dataprocessinglist_tag, (pit->getCVParams.getCVTerms(), UInt(4));  //writeCVParams_(String& s, const Map< String, std::vector < CVTerm > > & , UInt indent)
          dataprocessinglist_tag += "\t\t\t\t<userParam name=\"" + String(DataProcessing::NamesOfProcessingAction[*pit]) + "\" value=\"" + String(dit->getSoftware().getName()) + "\" />\n";
          dataprocessinglist_tag += "\t\t\t</ProcessingMethod>\n";
        }
        dataprocessinglist_tag += "\t\t</DataProcessing>\n";
      }

      dataprocessinglist_tag += "\t</DataProcessingList>\n";

      softwarelist_tag += "\t</SoftwareList>\n";

      // Ratios tag
      String ratio_xml;
      switch (cmsq_->getAnalysisSummary().quant_type_)
      {
      case 0:
        //~ register ratio elements in numden_r_ids_ and r_r_obj_
        for (std::vector<ConsensusMap>::const_iterator mit = cmsq_->getConsensusMaps().begin(); mit != cmsq_->getConsensusMaps().end(); ++mit)
        {
          //~ std::vector< std::vector<UInt64> > cmid;
          for (ConsensusMap::const_iterator cit = mit->begin(); cit != mit->end(); ++cit)
          {
            std::vector<ConsensusFeature::Ratio> rv = cit->getRatios();
            //~ for (std::vector<ConsensusFeature::Ratio>::const_iterator rit = cit->getRatios().begin(); rit != cit->getRatios().end(); ++rit)
            for (Size i = 0; i < rv.size(); ++i)
            {
              ConsensusFeature::Ratio robj(rv[i]);
              //~ String rd = rit->numerator_ref_ + rit->denominator_ref_; // add ratiocalculation params too?
              String rd = robj.numerator_ref_ + robj.denominator_ref_; // add ratiocalculation params too?
              String tid = String(UniqueIdGenerator::getUniqueId());
              numden_r_ids_.insert(std::make_pair(rd, tid));

              //~ ConsensusFeature::Ratio robj(*rit); this segfaults!!! why???? oh, why?!?!?!?!
              r_r_obj_.insert(std::make_pair(tid, robj));
            }
          }
        }

        ratio_xml += "\t<RatioList>\n";
        for (std::map<String, String>::const_iterator rit = numden_r_ids_.begin(); rit != numden_r_ids_.end(); ++rit)
        {
          ratio_xml += "\t\t<Ratio id=\"r_" + String(rit->second) + "\" numerator_ref=\"a_" + String(r_r_obj_[rit->second].numerator_ref_) + "\" denominator_ref=\"a_" + String(r_r_obj_[rit->second].denominator_ref_) + "\" >\n";
          ratio_xml += "\t\t\t<RatioCalculation>\n";
          for (std::vector<String>::const_iterator dit = r_r_obj_[rit->second].description_.begin(); dit != r_r_obj_[rit->second].description_.end(); ++dit)
          {
            ratio_xml += "\t\t\t\t<userParam name=\"" + String(*dit) + "\"/>\n";
          }
          ratio_xml += "\t\t\t\t<cvParam cvRef=\"PSI-MS\" accession=\"MS:1001848\" name=\"simple ratio of two values\"/>\n";
          ratio_xml += "\t\t\t</RatioCalculation>\n";
          ratio_xml += "\t\t\t<NumeratorDataType>\n\t\t\t\t<cvParam accession=\"MS:1001847\" cvRef=\"PSI-MS\" name=\"reporter ion intensity\"/>\n\t\t\t</NumeratorDataType>\n";
          ratio_xml += "\t\t\t<DenominatorDataType>\n\t\t\t\t<cvParam accession=\"MS:1001847\" cvRef=\"PSI-MS\" name=\"reporter ion intensity\"/>\n\t\t\t</DenominatorDataType>\n";
          ratio_xml += "\t\t</Ratio>\n";
        }
        ratio_xml += "\t</RatioList>\n";
        break;

      case 1:
        break; //TODO for SILACAnalyzer to produce some ratios

      case 2:
        break; //no tool yet

      case 3:
        break; //why SIZE_OF_QUANT_TYPES anyway?
      }

      String glob_rfgr;
      // Assay & StudyVariables: each  "channel" gets its assay - each assay its rawfilegroup
      String assay_xml("\t<AssayList id=\"assaylist1\">\n"), study_xml("\t<StudyVariableList>\n"), inputfiles_xml("\t<InputFiles>\n");
      std::map<String, String> files;
      for (std::vector<MSQuantifications::Assay>::const_iterator ait = cmsq_->getAssays().begin(); ait != cmsq_->getAssays().end(); ++ait)
      {
        String rfgr, ar, vr;
        rfgr = String(UniqueIdGenerator::getUniqueId());
        vr = String(UniqueIdGenerator::getUniqueId());
        //TODO regroup at Rawfilesgroup level
        String rgs;
        bool group_exists = true;
        rgs += "\t\t<RawFilesGroup id=\"rfg_" + rfgr + "\">\n";
        for (std::vector<ExperimentalSettings>::const_iterator iit = ait->raw_files_.begin(); iit != ait->raw_files_.end(); ++iit)
        {
          if (files.find(iit->getLoadedFilePath()) == files.end())
          {
            group_exists = false;
            glob_rfgr = rfgr; //TODO remove that when real rawfile grouping is done
            UInt64 rid = UniqueIdGenerator::getUniqueId();
            files.insert(std::make_pair(iit->getLoadedFilePath(), rfgr));
            rgs += "\t\t\t<RawFile id=\"r_" + String(rid)  + "\" location=\"" + iit->getLoadedFilePath() + "\"/>\n";
            // TODO write proteowizards sourcefiles (if there is any mentioning of that in the mzml) into OpenMS::ExperimentalSettings of the exp
          }
          else
          {
            rfgr = String(files.find(iit->getLoadedFilePath())->second);
          }
          //~ what about the other experimentalsettings?
        }
        rgs += "\t\t</RawFilesGroup>\n";

        if (!group_exists)
        {
          inputfiles_xml += rgs;
        }

        assay_xml += "\t\t<Assay id=\"a_" + String(ait->uid_)  + "\" rawFilesGroup_ref=\"rfg_" + rfgr + "\">\n";
        assay_xml += "\t\t\t<Label>\n";

        switch (cmsq_->getAnalysisSummary().quant_type_) //enum QUANT_TYPES {MS1LABEL=0, MS2LABEL, LABELFREE, SIZE_OF_QUANT_TYPES}; // derived from processing applied
        {
        case 0:
          for (std::vector<std::pair<String, double> >::const_iterator lit = ait->mods_.begin(); lit != ait->mods_.end(); ++lit)
          {
            String cv_acc, cv_name;
            switch ((int)std::floor(lit->second + (double)0.5)) //delta >! 0
            {
            case 6:
              cv_acc = "MOD:00544";
              cv_name = "6x(13)C labeled residue";
              break;

            case 8:
              cv_acc = "MOD:00582";
              cv_name = "6x(13)C,2x(15)N labeled L-lysine";
              break;

            case 10:
              cv_acc = "MOD:00587";
              cv_name = "6x(13)C,4x(15)N labeled L-arginine";
              break;

            default:
              cv_name = "unlabeled sample";
              cv_acc = "MS:1002038";
            }
            assay_xml += "\t\t\t\t<Modification massDelta=\"" + String(lit->second) + "\" >\n";
            assay_xml += "\t\t\t\t\t<cvParam cvRef=\"PSI-MOD\" accession=\"" + cv_acc + "\" name=\"" + cv_name + "\" value=\"" + String(lit->first) + "\"/>\n";
            assay_xml += "\t\t\t\t</Modification>\n";
          }
          break;

        case 1:
        {
          //~ assay_xml += "\t\t\t\t<Modification massDelta=\"145\" residues=\"N-term\">\n";
          //~ assay_xml += "\t\t\t\t\t<cvParam name =\"itraq label\"/>\n";
          for (std::vector<std::pair<String, double> >::const_iterator lit = ait->mods_.begin(); lit != ait->mods_.end(); ++lit)
          {
            assay_xml += "\t\t\t\t<Modification massDelta=\"145\">\n";
            String cv_acc, cv_name;
            switch ((int)lit->second) //~ TODO 8plex
            {
            case 114:
              cv_name = "iTRAQ4plex-114 reporter fragment";
              cv_acc = "MOD:01522";
              break;

            case 115:
              cv_name = "iTRAQ4plex-115 reporter fragment";
              cv_acc = "MOD:01523";
              break;

            case 116:
              cv_name = "iTRAQ4plex-116 reporter fragment";
              cv_acc = "MOD:01524";
              break;

            case 117:
              cv_name = "iTRAQ4plex-117, mTRAQ heavy, reporter fragment";
              cv_acc = "MOD:01525";
              break;

            default:
              cv_name = "Applied Biosystems iTRAQ(TM) multiplexed quantitation chemistry";
              cv_acc = "MOD:00564";
            }
            assay_xml += "\t\t\t\t\t<cvParam cvRef=\"PSI-MOD\" accession=\"" + cv_acc +  "\" name=\"" + cv_name + "\" value=\"" + String(lit->first) + "\"/>\n";
            assay_xml += "\t\t\t\t</Modification>\n";
          }
          break;
        }

        default:
          assay_xml += "\t\t\t\t<Modification massDelta=\"0\">\n";
          assay_xml += "\t\t\t\t\t<cvParam name =\"no label\"/>\n";
          assay_xml += "\t\t\t\t</Modification>\n";
        }

        assay_xml += "\t\t\t</Label>\n";
        assay_xml += "\t\t</Assay>\n";

        // for SILACAnalyzer/iTRAQAnalyzer one assay is one studyvariable, this may change!!! TODO for iTRAQ
        study_xml += "\t<StudyVariable id=\"v_" + vr + "\" name=\"noname\">\n";
        study_xml += "\t\t\t<Assay_refs>a_" + String(ait->uid_) + "</Assay_refs>\n";
        study_xml += "\t</StudyVariable>\n";
      }
      assay_xml += "\t</AssayList>\n";

      inputfiles_xml += idfile_tag;
      inputfiles_xml += "\t</InputFiles>\n";
      study_xml += "\t</StudyVariableList>\n";
      os << inputfiles_xml << softwarelist_tag << dataprocessinglist_tag << assay_xml << study_xml << ratio_xml;

      // Features and QuantLayers
      std::vector<UInt64> fid;
      std::vector<Real> fin, fwi /*, fqu */;
      std::vector<std::vector<std::vector<UInt64> >  > cid; //per consensusmap - per consensus - per feature (first entry is consensus idref)
      std::vector<std::vector<Real> > f2i;
      String /* ratio_xml,  */ peptide_xml, feature_xml = "";
      feature_xml += "\t<FeatureList id=\"featurelist1\" rawFilesGroup_ref=\"rfg_" + glob_rfgr + "\">\n"; //TODO make registerExperiment also register the consensusmaps (and featuremaps) - keep the grouping with ids
      for (std::vector<ConsensusMap>::const_iterator mit = cmsq_->getConsensusMaps().begin(); mit != cmsq_->getConsensusMaps().end(); ++mit)
      {
        std::vector<std::vector<UInt64> > cmid;
        for (ConsensusMap::const_iterator cit = mit->begin(); cit != mit->end(); ++cit)
        {
          const std::set<FeatureHandle, FeatureHandle::IndexLess>& feature_handles = cit->getFeatures();
          switch (cmsq_->getAnalysisSummary().quant_type_) //enum QUANT_TYPES {MS1LABEL=0, MS2LABEL, LABELFREE, SIZE_OF_QUANT_TYPES}; // derived from processing applied
          {
          case 0: //ms1label
          {
            std::vector<UInt64> idvec;
            idvec.push_back(UniqueIdGenerator::getUniqueId());
            for (std::set<FeatureHandle, FeatureHandle::IndexLess>::const_iterator fit = feature_handles.begin(); fit != feature_handles.end(); ++fit)
            {
              fid.push_back(UniqueIdGenerator::getUniqueId());
              idvec.push_back(fid.back());
              fin.push_back(fit->getIntensity());
              fwi.push_back(fit->getWidth());
              //~ fqu.push_back(jt->getQuality());
              feature_xml += "\t\t<Feature id=\"f_" + String(fid.back()) + "\" rt=\"" + String(fit->getRT()) + "\" mz=\"" + String(fit->getMZ()) + "\" charge=\"" + String(fit->getCharge()) + "\">\n";
              // TODO as soon as SILACanalyzer incorporate convex hulls read from the featuremap
              //~ writeUserParam_(os, *jt, UInt(2)); // FeatureHandle has no MetaInfoInterface!!!
              feature_xml += "\t\t\t<userParam name=\"map_index\" value=\"" + String(fit->getMapIndex()) + "\"/>\n";
              feature_xml += "\t\t\t<userParam name=\"feature_index\" value=\"" + String(fit->getUniqueId()) + "\"/>\n";
              feature_xml += "\t\t</Feature>\n";
            }
            cmid.push_back(idvec);
          } break;

          case 1: //ms2label
          {
            std::vector<Real> fi;
            fid.push_back(UniqueIdGenerator::getUniqueId());
            feature_xml += "\t\t<Feature id=\"f_" + String(fid.back()) + "\" rt=\"" + String(cit->getRT()) + "\" mz=\"" + String(cit->getMZ()) + "\" charge=\"" + String(cit->getCharge()) + "\"/>\n";
            //~ std::vector<UInt64> cidvec;
            //~ cidvec.push_back(fid.back());
            for (std::set<FeatureHandle, FeatureHandle::IndexLess>::const_iterator fit = feature_handles.begin(); fit != feature_handles.end(); ++fit)
            {
              fi.push_back(fit->getIntensity());
            }
            f2i.push_back(fi);
          } break;

          case 2:
            break; //no tool yet

          case 3:
            break; //why SIZE_OF_QUANT_TYPES anyway?
          }
        }
        cid.push_back(cmid);
      }

      switch (cmsq_->getAnalysisSummary().quant_type_) //enum QUANT_TYPES {MS1LABEL=0, MS2LABEL, LABELFREE, SIZE_OF_QUANT_TYPES}; // derived from processing applied
      {
      case 0: //ms1label
      {
        feature_xml += String("\t\t<FeatureQuantLayer id=\"") + String("q_") + String(UniqueIdGenerator::getUniqueId()) + String("\">\n\t\t\t<ColumnDefinition>\n");
        //what featurehandle is capable of reporting
        feature_xml += String("\t\t\t\t<Column index=\"0\">\n\t\t\t\t\t<DataType>\n\t\t\t\t\t\t<cvParam cvRef=\"PSI-MS\" accession=\"MS:1001141\" name=\"intensity of precursor ion\"/>\n\t\t\t\t\t</DataType>\n\t\t\t\t</Column>");
        feature_xml += String("\t\t\t\t<Column index=\"1\">\n\t\t\t\t\t<DataType>\n\t\t\t\t\t\t<cvParam cvRef=\"PSI-MS\" accession=\"MS:1000086\" name=\"full width at half-maximum\"/>\n\t\t\t\t\t</DataType>\n\t\t\t\t</Column>"); // TODO make FWHM CV also quantification datatype
        //~ os << "\t\t\t\t<Column index=\"0\">\n\t\t\t\t\t<DataType>\n\t\t\t\t\t\t<cvParam cvRef=\"PSI-MS\" accession=\"TODO\" name=\"quality\"/>\n\t\t\t\t\t</DataType>\n\t\t\t\t</Column>"; // getQuality erst ab BaseFeature - nicht in FeatureHandle
        feature_xml += String("\n\t\t\t</ColumnDefinition>\n\t\t\t\t<DataMatrix>\n");
        for (Size i = 0; i < fid.size(); ++i)
        {
          feature_xml += String("\t\t\t\t\t<Row object_ref=\"f_") + String(fid[i]) + String("\">");
          feature_xml += String(fin[i]) + String(" ") + String(fwi[i]) /* + " " << fiq[i] */;
          feature_xml += String("</Row>\n");
        }
        feature_xml += String("\t\t\t</DataMatrix>\n");
        feature_xml += String("\t\t</FeatureQuantLayer>\n");
      }
      break;

      case 1: //ms2label
      {
        feature_xml += String("\t\t<MS2AssayQuantLayer id=\"ms2ql_") + String(UniqueIdGenerator::getUniqueId()) + String("\">\n\t\t\t<DataType>\n\t\t\t\t<cvParam cvRef=\"PSI-MS\" accession=\"MS:1001847\" name=\"reporter ion intensity\"/>\n\t\t\t</DataType>\n\t\t\t<ColumnIndex>");
        for (std::vector<MSQuantifications::Assay>::const_iterator ait = cmsq_->getAssays().begin(); ait != cmsq_->getAssays().end(); ++ait)
        {
          feature_xml += String("a_") + String(ait->uid_) + String(" ");
        }
        feature_xml += String("</ColumnIndex>\n\t\t\t<DataMatrix>\n");
        for (Size i = 0; i < fid.size(); ++i)
        {
          feature_xml += String("\t\t\t\t\t<Row object_ref=\"f_") + String(fid[i]) + String("\">");
          for (Size j = 0; j < f2i[i].size(); ++j)
          {
            feature_xml += String(f2i[i][j]) + " ";
          }
          feature_xml += String("</Row>\n");
        }
        feature_xml += String("\t\t\t</DataMatrix>\n\t\t</MS2AssayQuantLayer>\n");
      }
      break;

      case 2:
        break; //no tool yet

      case 3:
        break; //why SIZE_OF_QUANT_TYPES anyway?
      }
      feature_xml += String("\t</FeatureList>\n");

      // Peptides
      for (Size k = 0; k < cid.size(); ++k)
      {
        switch (cmsq_->getAnalysisSummary().quant_type_) //enum QUANT_TYPES {MS1LABEL=0, MS2LABEL, LABELFREE, SIZE_OF_QUANT_TYPES}; // derived from processing applied
        {
        case 0: // ms1label - iterate consensusmap?
        {
          peptide_xml += String("\t<PeptideConsensusList  finalResult=\"true\" id=\"") + String("m_") + String(UniqueIdGenerator::getUniqueId()) + String("\">\n"); //URGENT TODO evidenceref
          for (Size i = 0; i < cid[k].size(); ++i)
          {
            peptide_xml += String("\t\t<PeptideConsensus id=\"") + String("c_") + String(cid[k][i].front()) + String("\" charge=\"") + String((*cmsq_).getConsensusMaps()[k][i].getCharge()) + String("\">\n");
            for (Size j = 1; j < cid[k][i].size(); ++j)
            {
              peptide_xml += String("\t\t\t<EvidenceRef feature_ref=\"f_") + String(cid[k][i][j]) + String("\" assay_refs=\"a_") + String(cmsq_->getAssays()[(j - 1)].uid_) + String("\"/>\n");
            }
            if (!(*cmsq_).getConsensusMaps()[k][i].getPeptideIdentifications().empty())
            {
              //~ peptide_xml += "\t\t\t<IdentificationRef id_refs=\"";
              //~ peptide_xml += (*cmsq_).getConsensusMaps()[k][i].getPeptideIdentifications().front().getIdentifier() + "\" feature_refs=\"";
              //~ for (Size j=1; j < cid[k][i].size(); ++j)
              //~ {
              //~ peptide_xml += "f_" + cid[k][i][j]+ " ";
              //~ }
              //~ peptide_xml += (*cmsq_).getConsensusMaps()[k][i].getPeptideIdentifications().front().getIdentifier() + "\" identificationFile_ref=\"";
              //~ peptide_xml += idid_to_idfilenames.begin()->first  + "\"/>\n";
            }
            peptide_xml += String("\t\t</PeptideConsensus>\n");
          }

          // QuantLayers
          peptide_xml += String("\t\t<RatioQuantLayer id=\"q_") + String(UniqueIdGenerator::getUniqueId()) + String("\">\n");
          peptide_xml += String("\t\t\t\t\t<DataType>\n\t\t\t\t\t\t<cvParam cvRef=\"PSI-MS\" accession=\"MS:1001132\" name=\"peptide ratio\"/>\n\t\t\t\t\t</DataType>\n");
          peptide_xml += String("\t\t\t\t<ColumnIndex>");
          for (std::map<String, String>::const_iterator rit = numden_r_ids_.begin(); rit != numden_r_ids_.end(); ++rit)
          {
            peptide_xml += String("r_") + String(rit->second) + String(" ");
          }
          peptide_xml += String("</ColumnIndex>\n\t\t\t\t<DataMatrix>\n");

          //~ collect ratios
          for (Size i = 0; i < cid[k].size(); ++i)
          {
            peptide_xml += String("\t\t\t\t<Row object_ref=\"c_") + String(cid[k][i].front()) + String("\">");

            std::map<String, String> r_values;
            std::vector<ConsensusFeature::Ratio> temp_ratios = cmsq_->getConsensusMaps()[k][i].getRatios();
            for (std::vector<ConsensusFeature::Ratio>::const_iterator rit = temp_ratios.begin(); rit != temp_ratios.end(); ++rit)
            {
              String rd = rit->numerator_ref_ + rit->denominator_ref_;
              r_values.insert(std::make_pair(rd, String(rit->ratio_value_)));
            }
            std::vector<String> dis;
            //TODO isert missing ratio_refs into r_values with value "-1"
            for (std::map<String, String>::const_iterator sit = r_values.begin(); sit != r_values.end(); ++sit)
            {
              dis.push_back(sit->second);
            }
            peptide_xml += StringList(dis).concatenate(" ").trim() + String("</Row>\n");
          }
          peptide_xml += String("\t\t\t\t</DataMatrix>\n");
          peptide_xml += String("\t\t</RatioQuantLayer>\n");
          peptide_xml += String("\t</PeptideConsensusList>\n");
        }
        break;

        case 1: // ms2label
        {
          if (!searchdb_ref.empty() && k < 2) // would break if there is more than one consensusmap
          {
            String ass_refs;
            for (Size j = 0; j < cmsq_->getAssays().size(); ++j)
            {
              ass_refs += String("a_") + String(cmsq_->getAssays()[j].uid_) + String(" ");
            }
            ass_refs.trim();
            peptide_xml += String("\t<PeptideConsensusList  finalResult=\"false\" id=\"m_") + String(UniqueIdGenerator::getUniqueId()) + String("\">\n"); //URGENT TODO evidenceref
            for (Size i = 0; i < fid.size(); ++i)
            {
              if (!cmsq_->getConsensusMaps()[k][i].getPeptideIdentifications().empty())
              {
                peptide_xml += String("\t\t<PeptideConsensus id=\"c_") + String(UniqueIdGenerator::getUniqueId()) + String("\" charge=\"") + String(cmsq_->getConsensusMaps()[k][i].getCharge()) + String("\" searchDatabase_ref=\"") + searchdb_ref + String("\">\n");
                peptide_xml += String("\t\t\t<PeptideSequence>") + cmsq_->getConsensusMaps()[k][i].getPeptideIdentifications().front().getHits().front().getSequence().toUnmodifiedString() + String("</PeptideSequence>\n");
                peptide_xml += String("\t\t\t<EvidenceRef feature_ref=\"f_") + String(fid[i]) + String("\" assay_refs=\"") + ass_refs + String("\" id_refs=\"") + cmsq_->getConsensusMaps()[k][i].getPeptideIdentifications().front().getIdentifier() + String("\" identificationFile_ref=\"") + idfile_ref + String("\"/>\n");
                peptide_xml += String("\t\t</PeptideConsensus>\n");
              }
              //~ TODO ratios, when available (not yet for the iTRAQ tuples of iTRAQAnalyzer)
            }
            peptide_xml += String("\t</PeptideConsensusList>\n");
          }
        }
        break;

        case 2:
          break; //no tool yet

        case 3:
          break; //why SIZE_OF_QUANT_TYPES anyway?
        }
      }

      //--------------------------------------------------------------------------------------------
      // Proteins and Proteingroups
      //--------------------------------------------------------------------------------------------
      // TODO - omitted as there are no ids yet

      //~ os << ratio_xml;
      os << peptide_xml;
      os << feature_xml;

      os << "</QcML>\n";
    }

    void QcMLHandler::writeCVParams_(String& s, const Map<String, std::vector<CVTerm> >& cvl, UInt indent)
    {
      String inden((size_t)indent, '\t');
      for (std::map<String, std::vector<CVTerm> >::const_iterator jt = cvl.begin(); jt != cvl.end(); ++jt)
      {
        for (std::vector<CVTerm>::const_iterator kt =  (*jt).second.begin(); kt !=  (*jt).second.end(); ++kt)
        {
          s += inden;
          s += "<cvParam cvRef=\"" + kt->getCVIdentifierRef() + "\" accession=\"" + (*jt).first + "\" name=\"" + kt->getName();
          if (kt->hasValue())
          {
            s += "\" value=\"" + kt->getValue().toString() + "\"/>\n"; // value is OpenMS::DataValue
          }
          else
          {
            s +=     "\"/>\n";
          }
        }
      }
    }

    void QcMLHandler::writeUserParams_(std::ostream& os, const MetaInfoInterface& meta, UInt indent)
    {
      String h;
      writeUserParams_(h, meta, indent);
      os << h;
    }

    void QcMLHandler::writeUserParams_(String& s, const MetaInfoInterface& meta, UInt indent)
    {
      if (meta.isMetaEmpty())
      {
        return;
      }
      std::vector<String> keys;
      meta.getKeys(keys);

      for (Size i = 0; i != keys.size(); ++i)
      {
        s += String(indent, '\t') + "<userParam name=\"" + keys[i] + "\" unitName=\"";

        DataValue d = meta.getMetaValue(keys[i]);
        //determine type
        if (d.valueType() == DataValue::INT_VALUE)
        {
          s += "xsd:integer";
        }
        else if (d.valueType() == DataValue::DOUBLE_VALUE)
        {
          s += "xsd:double";
        }
        else //string or lists are converted to string
        {
          s += "xsd:string";
        }
        s += "\" value=\"" + (String)(d) + "\"/>" + "\n";
      }
    }

    void QcMLHandler::writeFeature_(ostream& os, const String& identifier_prefix, UInt64 identifier, UInt indentation_level)
    {
      //TODO: remove dummy
      os << "\n featurewriter: " << identifier_prefix << "-" << String(identifier) << "-" << String(indentation_level) << "\n";
      //TODO: remove dummy

      //~ String indent = String(indentation_level,'\t');

      //~ os << indent << "\t\t<Feature id=\"" << identifier_prefix << identifier ;
      //~ os << " RT=" << feat.getRT() << " MZ" << feat.getMZ();
      //~ os << " charge=" << feat.getCharge() << "\">\n";

      //~ // TODO dataprocessing_ref & assay_ref

      //~ os << "\t\t\t<Masstrace>";
      //~ vector<ConvexHull2D> hulls = feat.getConvexHulls();
      //~ vector<ConvexHull2D>::iterator citer = hulls.begin();
      //~ Size hulls_count = hulls.size();

      //~ for (Size i = 0;i < hulls_count; i++)
      //~ {
      //~ ConvexHull2D current_hull = hulls[i];
      //~ current_hull.compress();
      //~ Size hull_size = current_hull.getHullPoints().size();

      //~ for (Size j=0;j<hull_size;j++)
      //~ {
      //~ DPosition<2> pos = current_hull.getHullPoints()[j];
      //~ /*Size pos_size = pos.size();
      //~ os << indent << "\t\t\t\t<hullpoint>\n";
      //~ for (Size k=0; k<pos_size; k++)
      //~ {
      //~ os << indent << "\t\t\t\t\t<hposition dim=\"" << k << "\">" << precisionWrapper(pos[k]) << "</hposition>\n";
      //~ }
      //~ os << indent << "\t\t\t\t</hullpoint>\n";*/
      //~ os << indent << precisionWrapper(pos[0]) << " " << precisionWrapper(pos[1]) << " ";
      //~ }
      //~ os << "\n";
      //~ }
      //~ os << "</Masstrace>\n";

      //~ os << indent << "<userParam name=\"overallquality\" value=\"" << precisionWrapper(feat.getOverallQuality()) << "\" unitName=\"quality\""/>

      //~ /*TODO write model description as a userParam
      //~ ModelDescription<2> desc = feat.getModelDescription();
      //~ if (!desc.getName().empty() || !desc.getParam().empty())
      //~ {
      //~ os << indent << "\t\t\t<model name=\"" << desc.getName() << "\">\n";
      //~ Param modelp = desc.getParam();
      //~ Param::ParamIterator piter = modelp.begin();
      //~ while (piter != modelp.end())
      //~ {
      //~ os << indent << "\t\t\t\t<param name=\"" << piter.getName() << "\" value=\"" << piter->value << "\"/>\n";
      //~ piter++;
      //~ }
      //~ os << indent << "\t\t\t</model>\n";
      //~ } */
      //~ writeUserParam_(os, feat, indentation_level + 3);

      //~ os << indent << "\t\t</feature>\n";
    }

  } //namespace Internal
} // namespace OpenMS
