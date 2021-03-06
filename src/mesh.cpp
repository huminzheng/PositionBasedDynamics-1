//
// Created by Jack Purvis
//

#include <iostream>
#include <fstream>
#include <sstream>
#include <shaderLoader.hpp>
#include <mesh.hpp>

Mesh::Mesh(string filename, Vector3f colour, float inverseMass) : colour(colour), inverseMass(inverseMass) {
    parseObjFile(filename);

    initialVertices = vertices;

    // Setup VBO
    glGenBuffers(1, &positionVBO);
    glGenBuffers(1, &normalVBO);

    // Setup shader
    shader = loadShaders("SimpleVertexShader", "SimpleFragmentShader");

    // Setup simulation
    reset();

    // Setup bounding box
    boundingBox = new BoundingBox();
    updateBoundingBox();
}

Mesh::~Mesh() {
    delete boundingBox;
}

void Mesh::generateSurfaceNormals() {
    surfaceNormals.clear();
    for (int i = 0; i < numFaces; i++) {
        Vector3f v1 = vertices[triangles[i].v[1].p] - vertices[triangles[i].v[0].p];
        Vector3f v2 = vertices[triangles[i].v[2].p] - vertices[triangles[i].v[0].p];
        Vector3f surfaceNormal = v1.cross(v2);
        surfaceNormal.normalize();
        surfaceNormals.push_back(surfaceNormal);
    }
}

void Mesh::reset() {
    vertices = initialVertices;

    velocities.clear();
    velocities.resize((size_t) numVertices, Vector3f::Zero());
}

void Mesh::applyImpulse(Vector3f force) {
    for (int i = 0; i < numVertices; i++) {
        velocities[i] += force;
    }
}

void Mesh::translate(Vector3f translate) {
    for (int i = 0; i < numVertices; i++) {
        vertices[i] += translate;
    }
}

void Mesh::updateBoundingBox() {
    float xMin = INFINITY;
    float xMax = -INFINITY;
    float yMin = INFINITY;
    float yMax = -INFINITY;
    float zMin = INFINITY;
    float zMax = -INFINITY;

    // Compute the box that bounds the mesh
    for (int i = 0; i < numVertices; i++) {
        Vector3f vertex = vertices[i];

        if (vertex[0] < xMin) xMin = vertex[0];
        if (vertex[0] > xMax) xMax = vertex[0];

        if (vertex[1] < yMin) yMin = vertex[1];
        if (vertex[1] > yMax) yMax = vertex[1];

        if (vertex[2] < zMin) zMin = vertex[2];
        if (vertex[2] > zMax) zMax = vertex[2];
    }

    boundingBox->xMin = xMin;
    boundingBox->xMax = xMax;
    boundingBox->yMin = yMin;
    boundingBox->yMax = yMax;
    boundingBox->zMin = zMin;
    boundingBox->zMax = zMax;
}

bool Mesh::intersect(Vector3f rayOrigin, Vector3f rayDirection, float &t, Vector3f &normal, int vertexIndex, int &triHitIndex) {

    // Ensure the ray intersects the bounding box before testing each triangle
    if (!boundingBox->intersect(rayOrigin, rayDirection)) return false;

    bool hit = false;
    t = INFINITY;
    int closestIndex = 0;
    Vector2f uv;

    // Check each triangle to try and find an intersection
    for (int triangleIndex = 0; triangleIndex < numFaces; triangleIndex++) {
        float tTri = INFINITY;
        float u, v;
        if (rayTriangleIntersect(rayOrigin, rayDirection, tTri, triangleIndex, vertexIndex, u, v) && fabs(tTri) < fabs(t)) {
            hit = true;
            t = tTri;
            closestIndex = triangleIndex;
            uv[0] = u;
            uv[1] = v;
        }
    }

    if (hit) {
        triHitIndex = closestIndex;

        // Compute the normal at the intersected triangle
        Vector3f v1 = vertices[triangles[closestIndex].v[1].p] - vertices[triangles[closestIndex].v[0].p];
        Vector3f v2 = vertices[triangles[closestIndex].v[2].p] - vertices[triangles[closestIndex].v[0].p];
        normal = v1.cross(v2);
        normal.normalize();
    }

    return hit;
}

bool Mesh::rayTriangleIntersect(Vector3f rayOrigin, Vector3f rayDirection, float &t, int triangleIndex, int vertexIndex, float &u, float &v) {
    int i0 = triangles[triangleIndex].v[0].p;
    int i1 = triangles[triangleIndex].v[1].p;
    int i2 = triangles[triangleIndex].v[2].p;

    if (i0 == vertexIndex || i1 == vertexIndex || i2 == vertexIndex) return false;

    // Get the triangle properties
    Vector3f v0 = vertices[i0];
    Vector3f v1 = vertices[i1];
    Vector3f v2 = vertices[i2];

    Vector3f v0v1 = v1 - v0;
    Vector3f v0v2 = v2 - v0;
    Vector3f pvec = rayDirection.cross(v0v2);
    float det = v0v1.dot(pvec);

    // Check if ray and triangle are parallel
    if (fabs(det) < 0.0001f) return false;

    float invDet = 1.0f / det;

    Vector3f tvec = rayOrigin - v0;
    u = tvec.dot(pvec) * invDet;
    if (u < 0 || u > 1) return false;

    Vector3f qvec = tvec.cross(v0v1);
    v = rayDirection.dot(qvec) * invDet;
    if (v < 0 || u + v > 1) return false;

    t = v0v2.dot(qvec) * invDet;

    return true;
}

void Mesh::render(Camera* camera, Matrix4f transform) {

    // Setup transform
    Affine3f t(Translation3f(position[0], position[1], position[2]));
    Matrix4f modelMatrix = transform * t.matrix();

    // Compute vertex normals
    vector<Vector3f> tempNormals;
    tempNormals.resize((size_t) numVertices, Vector3f::Zero());
    generateSurfaceNormals();
    for (unsigned int i = 0; i < numFaces; i++) {
        Triangle tri = triangles[i];

        for (int j = 0; j < 3; j++) {
            tempNormals[tri.v[j].p] += surfaceNormals[i];
        }
    }

    // Build vertex positions and normals
    vector<Vector3f> outVertices;
    vector<Vector3f> outNormals;
    for (unsigned int i = 0; i < numFaces; i++) {
        Triangle tri = triangles[i];

        for (int j = 0; j < 3; j++) {
            Vector3f position = vertices[tri.v[j].p];
            Vector3f normal = tempNormals[tri.v[j].p];

            outVertices.push_back(position);
            normal.normalize();
            outNormals.push_back(normal);
        }
    }

    glBindBuffer(GL_ARRAY_BUFFER, positionVBO);
    glBufferData(GL_ARRAY_BUFFER, outVertices.size() * sizeof(Vector3f), outVertices[0].data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, normalVBO);
    glBufferData(GL_ARRAY_BUFFER, outNormals.size() * sizeof(Vector3f), outNormals[0].data(), GL_STATIC_DRAW);

    glUseProgram(shader);

    glUniform3fv(glGetUniformLocation(shader, "materialColour"), 1, colour.data());
    Vector4f lightPosition = Vector4f(8, 10, 0, 0);
    lightPosition = modelMatrix * lightPosition;
    glUniform3fv(glGetUniformLocation(shader, "lightPosition"), 1, lightPosition.data());

    // Bind matrices
    glUniformMatrix4fv(glGetUniformLocation(3, "projection"), 1, GL_FALSE, camera->projectionMatrix.data());
    glUniformMatrix4fv(glGetUniformLocation(3, "view"), 1, GL_FALSE, camera->viewMatrix.data());
    glUniformMatrix4fv(glGetUniformLocation(3, "model"), 1, GL_FALSE, modelMatrix.data());

    // Bind vertex positions
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, positionVBO);
    glVertexAttribPointer(
            0,         // shader layout attribute
            3,         // size
            GL_FLOAT,  // type
            GL_FALSE,  // normalized?
            0,         // stride
            (void*)0   // array buffer offset
    );

    // Bind vertex normals
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, normalVBO);
    glVertexAttribPointer(
            1,         // shader layout attribute
            3,         // size
            GL_FLOAT,  // type
            GL_FALSE,  // normalized?
            0,         // stride
            (void*)0   // array buffer offset
    );

    glDrawArrays(GL_TRIANGLES, 0, outVertices.size());
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
}

void Mesh::parseObjFile(string filename) {

    // Attempt to open an input stream to the file
    ifstream objFile(filename);
    if (!objFile.is_open()) {
        cout << "Error reading " << filename << endl;
        return;
    }

    // While the file has lines remaining
    while (objFile.good()) {

        // Pull out line from file
        string line;
        getline(objFile, line);
        istringstream objLine(line);

        // Pull out mode from line
        string mode;
        objLine >> mode;

        // Reading like this means whitespace at the start of the line is fine
        // attempting to read from an empty string/line will set the failbit
        if (!objLine.fail()) {

            if (mode == "v") { // Vertex
                Vector3f v;
                objLine >> v[0] >> v[1] >> v[2];
                vertices.push_back(v);
            } else if (mode == "vn") { // Vertex normal
                Vector3f vn;
                objLine >> vn[0] >> vn[1] >> vn[2];
                normals.push_back(vn);
            } else if (mode == "vt") { // UV
                Vector2f vt;
                objLine >> vt[0] >> vt[1];
                uvs.push_back(vt);
            } else if (mode == "f") { // Face
                vector<Vertex> verts;
                while (objLine.good()) {
                    Vertex v;

                    // OBJ face is formatted as v/vt/vn
                    objLine >> v.p; // Scan in position index
                    objLine.ignore(1); // Ignore the '/' character
                    objLine >> v.t; // Scan in uv (texture coord) index
                    objLine.ignore(1); // Ignore the '/' character
                    objLine >> v.n; // Scan in normal index

                    // Correct the indices
                    v.p -= 1;
                    v.n -= 1;
                    v.t -= 1;

                    verts.push_back(v);
                }

                // If we have 3 vertices, construct a triangle
                if (verts.size() >= 3) {
                    Triangle triangle;
                    triangle.v[0] = verts[0];
                    triangle.v[1] = verts[1];
                    triangle.v[2] = verts[2];
                    triangles.push_back(triangle);

                    // Construct edges
                    Edge e1 = Edge(verts[0], verts[1]);
                    Edge e2 = Edge(verts[0], verts[2]);
                    Edge e3 = Edge(verts[1], verts[2]);
                    edges.insert(e1);
                    edges.insert(e2);
                    edges.insert(e3);

                    // Add to adjacent triangles
                    adjacentTriangles[e1].push_back(triangle);
                    adjacentTriangles[e2].push_back(triangle);
                    adjacentTriangles[e3].push_back(triangle);
                }
            }
        }
    }

    this->numVertices = (int) vertices.size();
    this->numFaces = (int) triangles.size();

    generateSurfaceNormals();
}
