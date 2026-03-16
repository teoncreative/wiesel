namespace WieselEngine
{
    public struct RaycastHit
    {
        public Entity Entity;
        public Vector3f Point;
        public Vector3f Normal;
        public float Distance;
    }

    public static class Physics
    {
        public static bool Raycast(ulong scenePtr, Vector3f origin, Vector3f direction, float maxDistance, out RaycastHit hit, ulong ignoreEntity = 0)
        {
            hit = new RaycastHit();
            ulong hitEntity = 0;
            float hitPx = 0, hitPy = 0, hitPz = 0;
            float hitNx = 0, hitNy = 0, hitNz = 0;
            float hitDist = 0;
            bool result = Internals.Physics_Raycast(scenePtr,
                origin.X, origin.Y, origin.Z,
                direction.X, direction.Y, direction.Z, maxDistance,
                ignoreEntity,
                out hitEntity, out hitPx, out hitPy, out hitPz,
                out hitNx, out hitNy, out hitNz, out hitDist);
            if (result)
            {
                hit.Entity = new Entity(scenePtr, hitEntity);
                hit.Point = new Vector3f(hitPx, hitPy, hitPz);
                hit.Normal = new Vector3f(hitNx, hitNy, hitNz);
                hit.Distance = hitDist;
            }
            return result;
        }

        public static Entity[] OverlapBox(ulong scenePtr, Vector3f center, Vector3f halfExtents)
        {
            ulong[] ids = Internals.Physics_OverlapBox(scenePtr,
                center.X, center.Y, center.Z,
                halfExtents.X, halfExtents.Y, halfExtents.Z);
            Entity[] entities = new Entity[ids.Length];
            for (int i = 0; i < ids.Length; i++)
            {
                entities[i] = new Entity(scenePtr, ids[i]);
            }
            return entities;
        }

        public static Entity[] OverlapSphere(ulong scenePtr, Vector3f center, float radius)
        {
            ulong[] ids = Internals.Physics_OverlapSphere(scenePtr,
                center.X, center.Y, center.Z, radius);
            Entity[] entities = new Entity[ids.Length];
            for (int i = 0; i < ids.Length; i++)
            {
                entities[i] = new Entity(scenePtr, ids[i]);
            }
            return entities;
        }
    }
}
