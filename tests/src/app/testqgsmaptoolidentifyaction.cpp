/***************************************************************************
     testqgsmaptoolidentifyaction.cpp
     --------------------------------
    Date                 : 2016-02-14
    Copyright            : (C) 2016 by Nyall Dawson
    Email                : nyall dot dawson at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgstest.h"

#include "qgsapplication.h"
#include "qgsvectorlayer.h"
#include "qgsrasterlayer.h"
#include "qgsfeature.h"
#include "qgsgeometry.h"
#include "qgsvectordataprovider.h"
#include "qgsproject.h"
#include "qgsmapcanvas.h"
#include "qgsunittypes.h"
#include "qgsmaptoolidentifyaction.h"
#include "qgssettings.h"

#include "cpl_conv.h"

class TestQgsMapToolIdentifyAction : public QObject
{
    Q_OBJECT
  public:
    TestQgsMapToolIdentifyAction()
      : canvas( 0 )
    {}

  private slots:
    void initTestCase(); // will be called before the first testfunction is executed.
    void cleanupTestCase(); // will be called after the last testfunction was executed.
    void init(); // will be called before each testfunction is executed.
    void cleanup(); // will be called after every testfunction.
    void lengthCalculation(); //test calculation of derived length attributes
    void perimeterCalculation(); //test calculation of derived perimeter attribute
    void areaCalculation(); //test calculation of derived area attribute
    void identifyRasterFloat32(); // test pixel identification and decimal precision
    void identifyRasterFloat64(); // test pixel identification and decimal precision
    void identifyInvalidPolygons(); // test selecting invalid polygons

  private:
    QgsMapCanvas *canvas = nullptr;

    QString testIdentifyRaster( QgsRasterLayer *layer, double xGeoref, double yGeoref );
    QList<QgsMapToolIdentify::IdentifyResult> testIdentifyVector( QgsVectorLayer *layer, double xGeoref, double yGeoref );

    // Release return with delete []
    unsigned char *
    hex2bytes( const char *hex, int *size )
    {
      QByteArray ba = QByteArray::fromHex( hex );
      unsigned char *out = new unsigned char[ba.size()];
      memcpy( out, ba.data(), ba.size() );
      *size = ba.size();
      return out;
    }

    // TODO: make this a QgsGeometry member...
    QgsGeometry geomFromHexWKB( const char *hexwkb )
    {
      int wkbsize;
      unsigned char *wkb = hex2bytes( hexwkb, &wkbsize );
      QgsGeometry geom;
      // NOTE: QgsGeometry takes ownership of wkb
      geom.fromWkb( wkb, wkbsize );
      return geom;
    }

};

void TestQgsMapToolIdentifyAction::initTestCase()
{
  QgsApplication::init();
  QgsApplication::initQgis();
  // Set up the QgsSettings environment
  QCoreApplication::setOrganizationName( QStringLiteral( "QGIS" ) );
  QCoreApplication::setOrganizationDomain( QStringLiteral( "qgis.org" ) );
  QCoreApplication::setApplicationName( QStringLiteral( "QGIS-TEST" ) );

  QgsApplication::showSettings();

  // enforce C locale because the tests expect it
  // (decimal separators / thousand separators)
  QLocale::setDefault( QLocale::c() );
}

void TestQgsMapToolIdentifyAction::cleanupTestCase()
{
  QgsApplication::exitQgis();
}

void TestQgsMapToolIdentifyAction::init()
{
  canvas = new QgsMapCanvas();
}

void TestQgsMapToolIdentifyAction::cleanup()
{
  delete canvas;
}

void TestQgsMapToolIdentifyAction::lengthCalculation()
{
  QgsSettings s;
  s.setValue( QStringLiteral( "/qgis/measure/keepbaseunit" ), true );

  //create a temporary layer
  std::unique_ptr< QgsVectorLayer> tempLayer( new QgsVectorLayer( QStringLiteral( "LineString?crs=epsg:3111&field=pk:int&field=col1:double" ), QStringLiteral( "vl" ), QStringLiteral( "memory" ) ) );
  QVERIFY( tempLayer->isValid() );
  QgsFeature f1( tempLayer->dataProvider()->fields(), 1 );
  f1.setAttribute( QStringLiteral( "pk" ), 1 );
  f1.setAttribute( QStringLiteral( "col1" ), 0.0 );
  QgsPolyline line3111;
  line3111 << QgsPointXY( 2484588, 2425722 ) << QgsPointXY( 2482767, 2398853 );
  QgsGeometry line3111G = QgsGeometry::fromPolyline( line3111 ) ;
  f1.setGeometry( line3111G );
  tempLayer->dataProvider()->addFeatures( QgsFeatureList() << f1 );

  // set project CRS and ellipsoid
  QgsCoordinateReferenceSystem srs( 3111, QgsCoordinateReferenceSystem::EpsgCrsId );
  canvas->setDestinationCrs( srs );
  canvas->setExtent( f1.geometry().boundingBox() );
  QgsProject::instance()->setCrs( srs );
  QgsProject::instance()->setEllipsoid( QStringLiteral( "WGS84" ) );
  QgsProject::instance()->setDistanceUnits( QgsUnitTypes::DistanceMeters );

  QgsPointXY mapPoint = canvas->getCoordinateTransform()->transform( 2484588, 2425722 );

  std::unique_ptr< QgsMapToolIdentifyAction > action( new QgsMapToolIdentifyAction( canvas ) );
  QList<QgsMapToolIdentify::IdentifyResult> result = action->identify( mapPoint.x(), mapPoint.y(), QList<QgsMapLayer *>() << tempLayer.get() );
  QCOMPARE( result.length(), 1 );
  QString derivedLength = result.at( 0 ).mDerivedAttributes[tr( "Length" )];
  double length = derivedLength.remove( ',' ).split( ' ' ).at( 0 ).toDouble();
  QVERIFY( qgsDoubleNear( length, 26932.2, 0.1 ) );

  //check that project units are respected
  QgsProject::instance()->setDistanceUnits( QgsUnitTypes::DistanceFeet );
  result = action->identify( mapPoint.x(), mapPoint.y(), QList<QgsMapLayer *>() << tempLayer.get() );
  QCOMPARE( result.length(), 1 );
  derivedLength = result.at( 0 ).mDerivedAttributes[tr( "Length" )];
  length = derivedLength.remove( ',' ).split( ' ' ).at( 0 ).toDouble();
  QVERIFY( qgsDoubleNear( length, 88360.1, 0.1 ) );

  //test unchecked "keep base units" setting
  s.setValue( QStringLiteral( "/qgis/measure/keepbaseunit" ), false );
  result = action->identify( mapPoint.x(), mapPoint.y(), QList<QgsMapLayer *>() << tempLayer.get() );
  QCOMPARE( result.length(), 1 );
  derivedLength = result.at( 0 ).mDerivedAttributes[tr( "Length" )];
  length = derivedLength.remove( ',' ).split( ' ' ).at( 0 ).toDouble();
  QVERIFY( qgsDoubleNear( length, 16.735, 0.001 ) );
}

void TestQgsMapToolIdentifyAction::perimeterCalculation()
{
  QgsSettings s;
  s.setValue( QStringLiteral( "/qgis/measure/keepbaseunit" ), true );

  //create a temporary layer
  std::unique_ptr< QgsVectorLayer> tempLayer( new QgsVectorLayer( QStringLiteral( "Polygon?crs=epsg:3111&field=pk:int&field=col1:double" ), QStringLiteral( "vl" ), QStringLiteral( "memory" ) ) );
  QVERIFY( tempLayer->isValid() );
  QgsFeature f1( tempLayer->dataProvider()->fields(), 1 );
  f1.setAttribute( QStringLiteral( "pk" ), 1 );
  f1.setAttribute( QStringLiteral( "col1" ), 0.0 );
  QgsPolyline polygonRing3111;
  polygonRing3111 << QgsPointXY( 2484588, 2425722 ) << QgsPointXY( 2482767, 2398853 ) << QgsPointXY( 2520109, 2397715 ) << QgsPointXY( 2520792, 2425494 ) << QgsPointXY( 2484588, 2425722 );
  QgsPolygon polygon3111;
  polygon3111 << polygonRing3111;
  QgsGeometry polygon3111G = QgsGeometry::fromPolygon( polygon3111 ) ;
  f1.setGeometry( polygon3111G );
  tempLayer->dataProvider()->addFeatures( QgsFeatureList() << f1 );

  // set project CRS and ellipsoid
  QgsCoordinateReferenceSystem srs( 3111, QgsCoordinateReferenceSystem::EpsgCrsId );
  canvas->setDestinationCrs( srs );
  canvas->setExtent( f1.geometry().boundingBox() );
  QgsProject::instance()->setCrs( srs );
  QgsProject::instance()->setEllipsoid( QStringLiteral( "WGS84" ) );
  QgsProject::instance()->setDistanceUnits( QgsUnitTypes::DistanceMeters );

  QgsPointXY mapPoint = canvas->getCoordinateTransform()->transform( 2484588, 2425722 );

  std::unique_ptr< QgsMapToolIdentifyAction > action( new QgsMapToolIdentifyAction( canvas ) );
  QList<QgsMapToolIdentify::IdentifyResult> result = action->identify( mapPoint.x(), mapPoint.y(), QList<QgsMapLayer *>() << tempLayer.get() );
  QCOMPARE( result.length(), 1 );
  QString derivedPerimeter = result.at( 0 ).mDerivedAttributes[tr( "Perimeter" )];
  double perimeter = derivedPerimeter.remove( ',' ).split( ' ' ).at( 0 ).toDouble();
  QCOMPARE( perimeter, 128289.074 );

  //check that project units are respected
  QgsProject::instance()->setDistanceUnits( QgsUnitTypes::DistanceFeet );
  result = action->identify( mapPoint.x(), mapPoint.y(), QList<QgsMapLayer *>() << tempLayer.get() );
  QCOMPARE( result.length(), 1 );
  derivedPerimeter = result.at( 0 ).mDerivedAttributes[tr( "Perimeter" )];
  perimeter = derivedPerimeter.remove( ',' ).split( ' ' ).at( 0 ).toDouble();
  QVERIFY( qgsDoubleNear( perimeter, 420896.0, 0.1 ) );

  //test unchecked "keep base units" setting
  s.setValue( QStringLiteral( "/qgis/measure/keepbaseunit" ), false );
  result = action->identify( mapPoint.x(), mapPoint.y(), QList<QgsMapLayer *>() << tempLayer.get() );
  QCOMPARE( result.length(), 1 );
  derivedPerimeter = result.at( 0 ).mDerivedAttributes[tr( "Perimeter" )];
  perimeter = derivedPerimeter.remove( ',' ).split( ' ' ).at( 0 ).toDouble();
  QVERIFY( qgsDoubleNear( perimeter, 79.715, 0.001 ) );
}

void TestQgsMapToolIdentifyAction::areaCalculation()
{
  QgsSettings s;
  s.setValue( QStringLiteral( "/qgis/measure/keepbaseunit" ), true );

  //create a temporary layer
  std::unique_ptr< QgsVectorLayer> tempLayer( new QgsVectorLayer( QStringLiteral( "Polygon?crs=epsg:3111&field=pk:int&field=col1:double" ), QStringLiteral( "vl" ), QStringLiteral( "memory" ) ) );
  QVERIFY( tempLayer->isValid() );
  QgsFeature f1( tempLayer->dataProvider()->fields(), 1 );
  f1.setAttribute( QStringLiteral( "pk" ), 1 );
  f1.setAttribute( QStringLiteral( "col1" ), 0.0 );

  QgsPolyline polygonRing3111;
  polygonRing3111 << QgsPointXY( 2484588, 2425722 ) << QgsPointXY( 2482767, 2398853 ) << QgsPointXY( 2520109, 2397715 ) << QgsPointXY( 2520792, 2425494 ) << QgsPointXY( 2484588, 2425722 );
  QgsPolygon polygon3111;
  polygon3111 << polygonRing3111;
  QgsGeometry polygon3111G = QgsGeometry::fromPolygon( polygon3111 ) ;
  f1.setGeometry( polygon3111G );
  tempLayer->dataProvider()->addFeatures( QgsFeatureList() << f1 );

  // set project CRS and ellipsoid
  QgsCoordinateReferenceSystem srs( 3111, QgsCoordinateReferenceSystem::EpsgCrsId );
  canvas->setDestinationCrs( srs );
  canvas->setExtent( f1.geometry().boundingBox() );
  QgsProject::instance()->setCrs( srs );
  QgsProject::instance()->setEllipsoid( QStringLiteral( "WGS84" ) );
  QgsProject::instance()->setAreaUnits( QgsUnitTypes::AreaSquareMeters );

  QgsPointXY mapPoint = canvas->getCoordinateTransform()->transform( 2484588, 2425722 );

  std::unique_ptr< QgsMapToolIdentifyAction > action( new QgsMapToolIdentifyAction( canvas ) );
  QList<QgsMapToolIdentify::IdentifyResult> result = action->identify( mapPoint.x(), mapPoint.y(), QList<QgsMapLayer *>() << tempLayer.get() );
  QCOMPARE( result.length(), 1 );
  QString derivedArea = result.at( 0 ).mDerivedAttributes[tr( "Area" )];
  double area = derivedArea.remove( ',' ).split( ' ' ).at( 0 ).toDouble();
  QVERIFY( qgsDoubleNear( area, 1009089817.0, 1.0 ) );

  //check that project units are respected
  QgsProject::instance()->setAreaUnits( QgsUnitTypes::AreaSquareMiles );
  result = action->identify( mapPoint.x(), mapPoint.y(), QList<QgsMapLayer *>() << tempLayer.get() );
  QCOMPARE( result.length(), 1 );
  derivedArea = result.at( 0 ).mDerivedAttributes[tr( "Area" )];
  area = derivedArea.remove( ',' ).split( ' ' ).at( 0 ).toDouble();
  QVERIFY( qgsDoubleNear( area, 389.6117, 0.001 ) );

  //test unchecked "keep base units" setting
  s.setValue( QStringLiteral( "/qgis/measure/keepbaseunit" ), false );
  QgsProject::instance()->setAreaUnits( QgsUnitTypes::AreaSquareFeet );
  result = action->identify( mapPoint.x(), mapPoint.y(), QList<QgsMapLayer *>() << tempLayer.get() );
  QCOMPARE( result.length(), 1 );
  derivedArea = result.at( 0 ).mDerivedAttributes[tr( "Area" )];
  area = derivedArea.remove( ',' ).split( ' ' ).at( 0 ).toDouble();
  QVERIFY( qgsDoubleNear( area, 389.6117, 0.001 ) );
}

// private
QString TestQgsMapToolIdentifyAction::testIdentifyRaster( QgsRasterLayer *layer, double xGeoref, double yGeoref )
{
  std::unique_ptr< QgsMapToolIdentifyAction > action( new QgsMapToolIdentifyAction( canvas ) );
  QgsPointXY mapPoint = canvas->getCoordinateTransform()->transform( xGeoref, yGeoref );
  QList<QgsMapToolIdentify::IdentifyResult> result = action->identify( mapPoint.x(), mapPoint.y(), QList<QgsMapLayer *>() << layer );
  if ( result.length() != 1 )
    return QLatin1String( "" );
  return result[0].mAttributes[QStringLiteral( "Band 1" )];
}

// private
QList<QgsMapToolIdentify::IdentifyResult>
TestQgsMapToolIdentifyAction::testIdentifyVector( QgsVectorLayer *layer, double xGeoref, double yGeoref )
{
  std::unique_ptr< QgsMapToolIdentifyAction > action( new QgsMapToolIdentifyAction( canvas ) );
  QgsPointXY mapPoint = canvas->getCoordinateTransform()->transform( xGeoref, yGeoref );
  QList<QgsMapToolIdentify::IdentifyResult> result = action->identify( mapPoint.x(), mapPoint.y(), QList<QgsMapLayer *>() << layer );
  return result;
}

void TestQgsMapToolIdentifyAction::identifyRasterFloat32()
{
  //create a temporary layer
  QString raster = QStringLiteral( TEST_DATA_DIR ) + "/raster/test.asc";

  // By default the QgsRasterLayer forces AAIGRID_DATATYPE=Float64
  CPLSetConfigOption( "AAIGRID_DATATYPE", "Float32" );
  std::unique_ptr< QgsRasterLayer> tempLayer( new QgsRasterLayer( raster ) );
  CPLSetConfigOption( "AAIGRID_DATATYPE", nullptr );

  QVERIFY( tempLayer->isValid() );

  canvas->setExtent( QgsRectangle( 0, 0, 7, 1 ) );

  QCOMPARE( testIdentifyRaster( tempLayer.get(), 0.5, 0.5 ), QString( "-999.9" ) );

  QCOMPARE( testIdentifyRaster( tempLayer.get(), 1.5, 0.5 ), QString( "-999.987" ) );

  // More than 6 significant digits for corresponding value in .asc:
  // precision loss in Float32
  QCOMPARE( testIdentifyRaster( tempLayer.get(), 2.5, 0.5 ), QString( "1.234568" ) ); // in .asc file : 1.2345678

  QCOMPARE( testIdentifyRaster( tempLayer.get(), 3.5, 0.5 ), QString( "123456" ) );

  // More than 6 significant digits: no precision loss here for that particular value
  QCOMPARE( testIdentifyRaster( tempLayer.get(), 4.5, 0.5 ), QString( "1234567" ) );

  // More than 6 significant digits: no precision loss here for that particular value
  QCOMPARE( testIdentifyRaster( tempLayer.get(), 5.5, 0.5 ), QString( "-999.9876" ) );

  // More than 6 significant digits for corresponding value in .asc:
  // precision loss in Float32
  QCOMPARE( testIdentifyRaster( tempLayer.get(), 6.5, 0.5 ), QString( "1.234568" ) ); // in .asc file : 1.2345678901234
}

void TestQgsMapToolIdentifyAction::identifyRasterFloat64()
{
  //create a temporary layer
  QString raster = QStringLiteral( TEST_DATA_DIR ) + "/raster/test.asc";
  std::unique_ptr< QgsRasterLayer> tempLayer( new QgsRasterLayer( raster ) );
  QVERIFY( tempLayer->isValid() );

  canvas->setExtent( QgsRectangle( 0, 0, 7, 1 ) );

  QCOMPARE( testIdentifyRaster( tempLayer.get(), 0.5, 0.5 ), QString( "-999.9" ) );

  QCOMPARE( testIdentifyRaster( tempLayer.get(), 1.5, 0.5 ), QString( "-999.987" ) );

  QCOMPARE( testIdentifyRaster( tempLayer.get(), 2.5, 0.5 ), QString( "1.2345678" ) );

  QCOMPARE( testIdentifyRaster( tempLayer.get(), 3.5, 0.5 ), QString( "123456" ) );

  QCOMPARE( testIdentifyRaster( tempLayer.get(), 4.5, 0.5 ), QString( "1234567" ) );

  QCOMPARE( testIdentifyRaster( tempLayer.get(), 5.5, 0.5 ), QString( "-999.9876" ) );

  QCOMPARE( testIdentifyRaster( tempLayer.get(), 6.5, 0.5 ), QString( "1.2345678901234" ) );
}

void TestQgsMapToolIdentifyAction::identifyInvalidPolygons()
{
  //create a temporary layer
  std::unique_ptr< QgsVectorLayer > memoryLayer( new QgsVectorLayer( QStringLiteral( "Polygon?field=pk:int" ), QStringLiteral( "vl" ), QStringLiteral( "memory" ) ) );
  QVERIFY( memoryLayer->isValid() );
  QgsFeature f1( memoryLayer->dataProvider()->fields(), 1 );
  f1.setAttribute( QStringLiteral( "pk" ), 1 );
  // This geometry is an invalid polygon (3 distinct vertices).
  // GEOS reported invalidity: Points of LinearRing do not form a closed linestring
  f1.setGeometry( geomFromHexWKB(
                    "010300000001000000030000000000000000000000000000000000000000000000000024400000000000000000000000000000244000000000000024400000000000000000"
                  ) );
  // TODO: check why we need the ->dataProvider() part, since
  //       there's a QgsVectorLayer::addFeatures method too
  //memoryLayer->addFeatures( QgsFeatureList() << f1 );
  memoryLayer->dataProvider()->addFeatures( QgsFeatureList() << f1 );

  canvas->setExtent( QgsRectangle( 0, 0, 10, 10 ) );
  QList<QgsMapToolIdentify::IdentifyResult> identified;
  identified = testIdentifyVector( memoryLayer.get(), 4, 6 );
  QCOMPARE( identified.length(), 0 );
  identified = testIdentifyVector( memoryLayer.get(), 6, 4 );
  QCOMPARE( identified.length(), 1 );
  QCOMPARE( identified[0].mFeature.attribute( "pk" ), QVariant( 1 ) );

}


QGSTEST_MAIN( TestQgsMapToolIdentifyAction )
#include "testqgsmaptoolidentifyaction.moc"




