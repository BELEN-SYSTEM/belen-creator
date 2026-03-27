-- ---------------------------------------------------------------------------
-- galeria (propiedad vía pieza)
-- ---------------------------------------------------------------------------
DROP POLICY IF EXISTS "galeria_select" ON public.galeria;
DROP POLICY IF EXISTS "galeria_insert" ON public.galeria;
DROP POLICY IF EXISTS "galeria_update" ON public.galeria;
DROP POLICY IF EXISTS "galeria_delete" ON public.galeria;

CREATE POLICY "galeria_select" ON public.galeria
  FOR SELECT TO authenticated
  USING (
    public.is_admin()
    OR (
      public.is_app_user()
      AND EXISTS (
        SELECT 1 FROM public.pieza p
        WHERE p.id = galeria.pieza_id AND p.user_id = auth.uid()
      )
    )
  );

CREATE POLICY "galeria_insert" ON public.galeria
  FOR INSERT TO authenticated
  WITH CHECK (
    public.is_admin()
    OR (
      public.is_app_user()
      AND EXISTS (
        SELECT 1 FROM public.pieza p
        WHERE p.id = galeria.pieza_id AND p.user_id = auth.uid()
      )
    )
  );

CREATE POLICY "galeria_update" ON public.galeria
  FOR UPDATE TO authenticated
  USING (
    public.is_admin()
    OR (
      public.is_app_user()
      AND EXISTS (
        SELECT 1 FROM public.pieza p
        WHERE p.id = galeria.pieza_id AND p.user_id = auth.uid()
      )
    )
  )
  WITH CHECK (
    public.is_admin()
    OR (
      public.is_app_user()
      AND EXISTS (
        SELECT 1 FROM public.pieza p
        WHERE p.id = galeria.pieza_id AND p.user_id = auth.uid()
      )
    )
  );

CREATE POLICY "galeria_delete" ON public.galeria
  FOR DELETE TO authenticated
  USING (
    public.is_admin()
    OR (
      public.is_app_user()
      AND EXISTS (
        SELECT 1 FROM public.pieza p
        WHERE p.id = galeria.pieza_id AND p.user_id = auth.uid()
      )
    )
  );
