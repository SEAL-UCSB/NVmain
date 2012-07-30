/*
 *  This file is part of NVMain- A cycle accurate timing, bit-accurate
 *  energy simulator for non-volatile memory. Originally developed by 
 *  Matt Poremba at the Pennsylvania State University.
 *
 *  Website: http://www.cse.psu.edu/~poremba/nvmain/
 *  Email: mrp5060@psu.edu
 *
 *  ---------------------------------------------------------------------
 *
 *  If you use this software for publishable research, please include 
 *  the original NVMain paper in the citation list and mention the use 
 *  of NVMain.
 *
 */

#ifndef __CONFIG_H__
#define __CONFIG_H__


#include <string>
#include <map>


#include "src/SimInterface.h"


namespace NVM {


class Config 
{
 public:
  Config ();
  ~Config ();

  Config(const Config& conf);
  
  void Read( std::string filename );
  std::string GetFileName( );

  int  GetValue( std::string key );
  void SetValue( std::string key, std::string value );

  float GetEnergy( std::string key );
  void  SetEnergy( std::string key, std::string energy );

  std::string GetString( std::string key );
  void  SetString( std::string key, std::string );

  bool KeyExists( std::string key );

  void Print( );

  /*
   *  Any special class to get information from the underlying
   *  simulator can be set here.
   */
  void SetSimInterface( SimInterface *simPtr );
  SimInterface *GetSimInterface( );

 private:
  std::string fileName;
  std::map<std::string, std::string> values;
  SimInterface *simPtr;

};


};


#endif


