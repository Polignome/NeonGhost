// NeonGhostQuickTestApp.cpp : Diese Datei enthält die Funktion "main". Hier beginnt und endet die Ausführung des Programms.
//

#include <iostream>
#include <NeonGhostCore.h>


int main()
{
	
	
	
	{
		std::vector<ng::math::Matrix<3, 1, float>> l;
		l.push_back(ng::math::Matrix<3, 1, float>(1, 2, 3));
		l.push_back(ng::math::Matrix<3, 1, float>(1, -5, 3));
		l.push_back(ng::math::Matrix<3, 1, float>(1, 2, -3));
		l.push_back(ng::math::Matrix<3, 1, float>(-3, 2, 8));
		ng::bounding::AABBox<3, float> aabb;

		aabb += l;
		printf(" %f %f %f  ->  %f %f %f\n", aabb.Min().x(), aabb.Min().y(), aabb.Min().z(), aabb.Max().x(), aabb.Max().y(), aabb.Max().z());
	}


	{
		std::vector< ng::geometry::Vertex<float>> l;
		l.push_back(ng::geometry::Vertex<float>(ng::math::Matrix<3, 1, float>(1, 2, 3)));
		l.push_back(ng::geometry::Vertex<float>(ng::math::Matrix<3, 1, float>(1, -5, 3)));
		l.push_back(ng::geometry::Vertex<float>(ng::math::Matrix<3, 1, float>(1, 2, -3)));
		l.push_back(ng::geometry::Vertex<float>(ng::math::Matrix<3, 1, float>(-3, 2, 8)));
		ng::bounding::AABBox<3, float> aabb;

		aabb += l;
		printf("warum? %f %f %f  ->  %f %f %f\n", aabb.Min().x(), aabb.Min().y(), aabb.Min().z(), aabb.Max().x(), aabb.Max().y(), aabb.Max().z());
	}

	
	{
		ng::bounding::AABBox<3,float> aabb;
		ng::bounding::BoundingSphere<float> sphere(ng::math::Matrix<3,1,float>(10,20,30),10);
		aabb += sphere;
		printf(" %f %f %f  ->  %f %f %f\n", aabb.Min().x(), aabb.Min().y(), aabb.Min().z(), aabb.Max().x(), aabb.Max().y(), aabb.Max().z());
		ng::bounding::BoundingSphere<float> sphere2;
		sphere2 += aabb;
		sphere2.Center();
		printf(" %f %f %f  ->  %f \n", sphere2.Center().x(), sphere2.Center().y(), sphere2.Center().z(), sphere2.Radius());
	}

	{
		ng::bounding::AABBox<3, float> aabb1(-10, -10, -10, 10, 10, 10);
		ng::bounding::AABBox<3, float> aabb2(-10 + 5, -10 + 5, -10 + 5, 10 + 5, 10 + 5, 10 + 5);
		ng::bounding::BoundingSphere<float> sphere(-15, -15, -15, 20);
		ng::bounding::BoundingSphere<float> sphere2(-25, -25, -25, 20);

		printf("Test aabb1 / abb2 %i   \n", aabb1.IntersectedByBounds(aabb2));
		printf("Test aabb1 / sphere1 %i\n", ng::bounding::Intersects(aabb1, sphere));
		printf("Test aabb1 / sphere2 %i\n", ng::bounding::Intersects(aabb1, sphere));
	}

	{
		std::vector< ng::geometry::Vertex<float>> l;
		l.push_back(ng::geometry::Vertex<float>(ng::math::Matrix<3, 1, float>(1, 2, 3)));
		l.push_back(ng::geometry::Vertex<float>(ng::math::Matrix<3, 1, float>(1, -5, 3)));
		l.push_back(ng::geometry::Vertex<float>(ng::math::Matrix<3, 1, float>(1, 2, -3)));
		l.push_back(ng::geometry::Vertex<float>(ng::math::Matrix<3, 1, float>(-3, 2, 8)));

		ng::geometry::Primitive<3, float> prim(l);

		ng::bounding::AABBox<3, float> aabb;

		aabb += prim;
		printf(" %f %f %f  ->  %f %f %f\n", aabb.Min().x(), aabb.Min().y(), aabb.Min().z(), aabb.Max().x(), aabb.Max().y(), aabb.Max().z());
	}

	{
		std::vector< ng::geometry::Vertex<float>> l;
		l.push_back(ng::geometry::Vertex<float>(ng::math::Matrix<3, 1, float>(1, 2, 3)));
		l.push_back(ng::geometry::Vertex<float>(ng::math::Matrix<3, 1, float>(1, -5, 3)));
		l.push_back(ng::geometry::Vertex<float>(ng::math::Matrix<3, 1, float>(1, 2, -3)));
		l.push_back(ng::geometry::Vertex<float>(ng::math::Matrix<3, 1, float>(-3, 2, 8)));

		std::vector<ng::geometry::Primitive<3, float>> prims;
		prims.push_back(ng::geometry::Primitive<3, float>(l));
		prims.push_back(ng::geometry::Primitive<3, float>(l));
		prims.push_back(ng::geometry::Primitive<3, float>(l));

		ng::bounding::AABBox<3, float> aabb;

		aabb += prims;
		printf(" Prims check %f %f %f  ->  %f %f %f\n", aabb.Min().x(), aabb.Min().y(), aabb.Min().z(), aabb.Max().x(), aabb.Max().y(), aabb.Max().z());
	}



}

// Programm ausführen: STRG+F5 oder Menüeintrag "Debuggen" > "Starten ohne Debuggen starten"
// Programm debuggen: F5 oder "Debuggen" > Menü "Debuggen starten"

// Tipps für den Einstieg: 
//   1. Verwenden Sie das Projektmappen-Explorer-Fenster zum Hinzufügen/Verwalten von Dateien.
//   2. Verwenden Sie das Team Explorer-Fenster zum Herstellen einer Verbindung mit der Quellcodeverwaltung.
//   3. Verwenden Sie das Ausgabefenster, um die Buildausgabe und andere Nachrichten anzuzeigen.
//   4. Verwenden Sie das Fenster "Fehlerliste", um Fehler anzuzeigen.
//   5. Wechseln Sie zu "Projekt" > "Neues Element hinzufügen", um neue Codedateien zu erstellen, bzw. zu "Projekt" > "Vorhandenes Element hinzufügen", um dem Projekt vorhandene Codedateien hinzuzufügen.
//   6. Um dieses Projekt später erneut zu öffnen, wechseln Sie zu "Datei" > "Öffnen" > "Projekt", und wählen Sie die SLN-Datei aus.
